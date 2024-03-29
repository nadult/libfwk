// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/parse.h"

#include "fwk/format.h"
#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"
#include "fwk/math/quat.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace fwk {

namespace {
	FWK_NO_INLINE void reportParseError(Str str, const char *type_name, int count) EXCEPT {
		if(quietExceptionsMode())
			raiseQuietException();
		else {
			string what = count > 1 ? stdFormat("%d %s", count, type_name) : type_name;
			auto short_str = str.limitSizeBack(40);
			RAISE("Error while parsing %% from \"%\"", what, count > 1 ? "s" : "", short_str);
		}
	}

	FWK_NO_INLINE void reportOutOfRange(Str str, const char *type_name) EXCEPT {
		if(quietExceptionsMode())
			raiseQuietException();
		else {
			auto short_str = str.limitSizeBack(40);
			RAISE("Error while parsing %: value out of range: \"%\"", type_name, short_str);
		}
	}

	auto strtol(const char *ptr, char **end_ptr) { return ::strtol(ptr, end_ptr, 0); }
	auto strtoul(const char *ptr, char **end_ptr) { return ::strtoul(ptr, end_ptr, 0); }
	auto strtoll(const char *ptr, char **end_ptr) { return ::strtoll(ptr, end_ptr, 0); }
	auto strtoull(const char *ptr, char **end_ptr) { return ::strtoull(ptr, end_ptr, 0); }
}

template <class Func> auto TextParser::parseSingle(Func func, const char *type_name) {
	const char *str = parseElement().data();
	char *end_ptr = nullptr;
	errno = 0;
	auto value = func(str, &end_ptr);
	if(errno != 0 || end_ptr == str)
		reportParseError(str, type_name, 1);
	return value;
}

template <class T, class Func> T TextParser::parseSingleRanged(Func func, const char *type_name) {
	auto element = parseElement();

	char *end_ptr = nullptr;
	errno = 0;
	auto value = func(element.data(), &end_ptr);
	if(errno != 0 || end_ptr != element.end())
		reportParseError(element, type_name, 1);
	else if(value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max())
		reportOutOfRange(element, type_name);
	return T(value);
}

int TextParser::countElements() const {
	int count = 0;
	const char *ptr = m_current.c_str();
	while(*ptr) {
		while(isspace(*ptr))
			ptr++;
		if(*ptr) {
			count++;
			while(*ptr && !isspace(*ptr))
				ptr++;
		}
	}
	return count;
}

Str TextParser::parseElement() {
	const char *ptr = m_current.data();

	// TODO: czy powinien przeskakiwać puste znaki na początku i na końcu?
	while(isspace(*ptr))
		ptr++;
	auto *start = ptr;
	while(!isspace(*ptr) && *ptr)
		ptr++;
	Str out(start, ptr);
	while(isspace(*ptr))
		ptr++;
	m_current = ZStr(ptr, m_current.end() - ptr);
	return out;
}

void TextParser::advanceWhitespace() {
	const char *ptr = m_current.c_str();
	while(isspace(*ptr))
		ptr++;
	m_current = ZStr(ptr, m_current.end() - ptr);
}

void TextParser::errorTrailingData() {
	RAISE("Trailing data left after parsing: '%'", current().limitSizeBack(20));
}

TextParser &TextParser::operator>>(Str &out) {
	out = parseElement();
	return *this;
}

TextParser &TextParser::operator>>(string &out) {
	out = parseElement();
	return *this;
}

TextParser &TextParser::operator>>(bool &out) {
	auto element = parseElement();

	if(element.compareIgnoreCase("true") == 0 || element == "1")
		out = true;
	else if(element.compareIgnoreCase("false") == 0 || element == "0")
		out = false;
	else {
		RAISE("Error while parsing bool from \"%\"", element);
		out = false;
	}

	return *this;
}

TextParser &TextParser::operator>>(double &out) {
	out = parseSingle(strtod, "double");
	return *this;
}

TextParser &TextParser::operator>>(float &out) {
	out = parseSingle(strtof, "float");
	return *this;
}

TextParser &TextParser::operator>>(short &out) {
	out = parseSingleRanged<short>(strtol, "short");
	return *this;
}

TextParser &TextParser::operator>>(unsigned short &out) {
	out = parseSingleRanged<unsigned short>(strtol, "unsigned short");
	return *this;
}

TextParser &TextParser::operator>>(int &out) {
	out = parseSingleRanged<int>(strtol, "int");
	return *this;
}

TextParser &TextParser::operator>>(unsigned int &out) {
	out = parseSingleRanged<unsigned int>(strtoul, "unsigned int");
	return *this;
}

TextParser &TextParser::operator>>(long &out) {
	out = parseSingleRanged<long>(strtol, "long");
	return *this;
}

TextParser &TextParser::operator>>(unsigned long &out) {
	out = parseSingleRanged<unsigned long>(strtoul, "unsigned long");
	return *this;
}

TextParser &TextParser::operator>>(long long &out) {
	out = parseSingleRanged<long long>(strtoll, "long long");
	return *this;
}

TextParser &TextParser::operator>>(unsigned long long &out) {
	out = parseSingleRanged<unsigned long long>(strtoull, "unsigned long long");
	return *this;
}

void TextParser::parseInts(Span<int> out) { parseSpan(out); }
void TextParser::parseFloats(Span<float> out) { parseSpan(out); }
void TextParser::parseDoubles(Span<double> out) { parseSpan(out); }

void TextParser::parseNotEmpty(Span<Str> out) {
	for(int n = 0; n < out.size(); n++) {
		*this >> out[n];
		if(out[n].empty()) {
			if(quietExceptionsMode())
				raiseQuietException();
			else
				RAISE("Error while parsing a range of % strings (parsed: %)", out.size(), n);
		}
	}
}

void TextParser::parseNotEmpty(Span<string> out) {
	for(int n = 0; n < out.size(); n++) {
		*this >> out[n];
		if(out[n].empty()) {
			if(quietExceptionsMode())
				raiseQuietException();
			else
				RAISE("Error while parsing a range of % strings (parsed: %)", out.size(), n);
		}
	}
}

TextParser &operator>>(TextParser &parser, short2 &out) { return parser >> out.x >> out.y; }

TextParser &operator>>(TextParser &parser, short3 &out) {
	return parser >> out.x >> out.y >> out.z;
}

TextParser &operator>>(TextParser &parser, short4 &out) {
	return parser >> out.x >> out.y >> out.z >> out.w;
}

TextParser &operator>>(TextParser &parser, int2 &out) {
	parser.parseInts(out.v);
	return parser;
}

TextParser &operator>>(TextParser &parser, int3 &out) {
	parser.parseInts(out.v);
	return parser;
}

TextParser &operator>>(TextParser &parser, int4 &out) {
	parser.parseInts(out.v);
	return parser;
}

TextParser &operator>>(TextParser &parser, double2 &out) {
	parser.parseDoubles(out.v);
	return parser;
}

TextParser &operator>>(TextParser &parser, double3 &out) {
	parser.parseDoubles(out.v);
	return parser;
}

TextParser &operator>>(TextParser &parser, double4 &out) {
	parser.parseDoubles(out.v);
	return parser;
}

TextParser &operator>>(TextParser &parser, float2 &out) {
	parser.parseFloats(out.v);
	return parser;
}

TextParser &operator>>(TextParser &parser, float3 &out) {
	parser.parseFloats(out.v);
	return parser;
}

TextParser &operator>>(TextParser &parser, float4 &out) {
	parser.parseFloats(out.v);
	return parser;
}

TextParser &operator>>(TextParser &parser, DRect &out) {
	double val[4];
	parser.parseDoubles(val);
	out = {val[0], val[1], val[2], val[3]};
	return parser;
}

TextParser &operator>>(TextParser &parser, FRect &out) {
	float val[4];
	parser.parseFloats(val);
	out = {val[0], val[1], val[2], val[3]};
	return parser;
}

TextParser &operator>>(TextParser &parser, IRect &out) {
	int val[4];
	parser.parseInts(val);
	out = {val[0], val[1], val[2], val[3]};
	return parser;
}

TextParser &operator>>(TextParser &parser, FBox &out) {
	float val[6];
	parser.parseFloats(val);
	out = {val[0], val[1], val[2], val[3], val[4], val[5]};
	return parser;
}

TextParser &operator>>(TextParser &parser, IBox &out) {
	int val[6];
	parser.parseInts(val);
	out = {val[0], val[1], val[2], val[3], val[4], val[5]};
	return parser;
}

TextParser &operator>>(TextParser &parser, DBox &out) {
	double val[6];
	parser.parseDoubles(val);
	out = {val[0], val[1], val[2], val[3], val[4], val[5]};
	return parser;
}

TextParser &operator>>(TextParser &parser, Matrix4 &out) {
	float val[16];
	parser.parseFloats(val);
	out = Matrix4(val);
	return parser;
}

TextParser &operator>>(TextParser &parser, Quat &out) {
	float val[4];
	parser.parseFloats(val);
	out = {val[0], val[1], val[2], val[3]};
	return parser;
}

TextParser &operator>>(TextParser &parser, vector<string> &out) {
	out.resize(parser.countElements());
	parser.parseSpan(out);
	return parser;
}

TextParser &operator>>(TextParser &parser, vector<int> &out) {
	out.resize(parser.countElements());
	parser.parseSpan(out);
	return parser;
}

TextParser &operator>>(TextParser &parser, vector<float> &out) {
	out.resize(parser.countElements());
	parser.parseSpan(out);
	return parser;
}
}
