// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_parse.h"

#include "fwk_format.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <functional>

namespace fwk {

namespace {

	static void throwError(const char *input, const char *type_name, int count) NOINLINE;
	void throwError(const char *input, const char *type_name, int count) {
		string what = count > 1 ? stdFormat("%d %s", count, type_name) : type_name;
		size_t max_len = 32;
		string short_input =
			strlen(input) > max_len ? string(input, input + max_len) + "..." : string(input);
		THROW("Error while parsing %s from \"%s\"", what.c_str(), short_input.c_str());
	}

	auto strtol(const char *ptr, char **end_ptr) { return ::strtol(ptr, end_ptr, 0); }
	auto strtoul(const char *ptr, char **end_ptr) { return ::strtoul(ptr, end_ptr, 0); }

	template <class Func> auto parseSingle(const char **ptr, Func func, const char *type_name) {
		char *end_ptr = const_cast<char *>(*ptr);
		errno = 0;
		auto value = func(*ptr, &end_ptr);
		if(errno != 0 || end_ptr == *ptr)
			throwError(*ptr, type_name, 1);
		*ptr = end_ptr;
		return value;
	}

	template <class Func, class T>
	void parseMultiple(const char **ptr, Span<T> out, Func func, const char *type_name) {
		char *end_ptr = const_cast<char *>(*ptr);
		errno = 0;
		for(int n = 0; n < out.size(); n++) {
			auto value = func(*ptr, &end_ptr);
			if(errno != 0 || end_ptr == *ptr)
				throwError(*ptr, type_name, out.size() - n);
			out[n] = value;
			*ptr = end_ptr;
		}
	}
}

int TextParser::countElements() const {
	int count = 0;
	const char *string = m_current;
	while(*string) {
		while(isspace(*string))
			string++;
		if(*string) {
			count++;
			while(*string && !isspace(*string))
				string++;
		}
	}
	return count;
}

bool TextParser::parseBool() {
	string str = parseString();
	std::transform(begin(str), end(str), begin(str), ::tolower);

	if(isOneOf(str, "true", "1"))
		return true;
	if(!isOneOf(str, "false", "0"))
		THROW("Error while parsing bool from \"%s\"", str.c_str());
	return false;
}

string TextParser::parseString() {
	string out[1];
	parseStrings(out);
	return out[0];
}

int TextParser::parseInt() { return parseSingle(&m_current, strtol, "int"); }
float TextParser::parseFloat() { return parseSingle(&m_current, strtof, "float"); }
double TextParser::parseDouble() { return parseSingle(&m_current, strtod, "double"); }
uint TextParser::parseUint() { return parseSingle(&m_current, strtoul, "uint"); }

void TextParser::parseInts(Span<int> out) { parseMultiple(&m_current, out, strtol, "int"); }
void TextParser::parseFloats(Span<float> out) { parseMultiple(&m_current, out, strtof, "float"); }
void TextParser::parseDoubles(Span<double> out) {
	parseMultiple(&m_current, out, strtod, "double");
}
void TextParser::parseUints(Span<uint> out) { parseMultiple(&m_current, out, strtoul, "uint"); }

void TextParser::parseStrings(Span<string> out) {
	// TODO: what if we have empty string on input?
	const char *iptr = m_current;
	int count = 0;

	while(*iptr && count < out.size()) {
		while(isspace(*iptr))
			iptr++;
		if(!*iptr)
			break;
		const char *start = iptr;
		while(*iptr && !isspace(*iptr))
			iptr++;
		out[count++] = string(start, iptr);
	}

	if(count < out.size())
		throwError(m_current, "string", out.size());
	m_current = iptr;
}

bool TextParser::hasAnythingLeft() {
	const char *ptr = m_current;
	while(isspace(*ptr))
		ptr++;
	m_current = ptr;
	return !isFinished();
}

namespace detail {

	template <> string fromString(TextParser &parser) { return parser.parseString(); }
	template <> bool fromString(TextParser &parser) { return parser.parseBool(); }
	template <> uint fromString(TextParser &parser) { return parser.parseUint(); }
	template <> int fromString(TextParser &parser) { return parser.parseInt(); }
	template <> float fromString(TextParser &parser) { return parser.parseFloat(); }
	template <> double fromString(TextParser &parser) { return parser.parseDouble(); }

	template <> int2 fromString(TextParser &parser) {
		int2 out;
		parser.parseInts(out.v);
		return out;
	}

	template <> int3 fromString(TextParser &parser) {
		int3 out;
		parser.parseInts(out.v);
		return out;
	}

	template <> int4 fromString(TextParser &parser) {
		int4 out;
		parser.parseInts(out.v);
		return out;
	}

	template <> float2 fromString(TextParser &parser) {
		float2 out;
		parser.parseFloats(out.v);
		return out;
	}

	template <> float3 fromString(TextParser &parser) {
		float3 out;
		parser.parseFloats(out.v);
		return out;
	}

	template <> float4 fromString(TextParser &parser) {
		float4 out;
		parser.parseFloats(out.v);
		return out;
	}

	template <> double2 fromString(TextParser &parser) {
		double2 out;
		parser.parseDoubles(out.v);
		return out;
	}

	template <> double3 fromString(TextParser &parser) {
		double3 out;
		parser.parseDoubles(out.v);
		return out;
	}

	template <> double4 fromString(TextParser &parser) {
		double4 out;
		parser.parseDoubles(out.v);
		return out;
	}

	template <> FRect fromString(TextParser &parser) {
		float out[4];
		parser.parseFloats(out);
		return {{out[0], out[1]}, {out[2], out[3]}};
	}

	template <> IRect fromString(TextParser &parser) {
		int out[4];
		parser.parseInts(out);
		return {{out[0], out[1]}, {out[2], out[3]}};
	}

	template <> DRect fromString(TextParser &parser) {
		double out[4];
		parser.parseDoubles(out);
		return {{out[0], out[1]}, {out[2], out[3]}};
	}

	template <> FBox fromString(TextParser &parser) {
		float out[6];
		parser.parseFloats(out);
		return {{out[0], out[1], out[2]}, {out[3], out[4], out[5]}};
	}

	template <> IBox fromString(TextParser &parser) {
		int out[6];
		parser.parseInts(out);
		return {{out[0], out[1], out[2]}, {out[3], out[4], out[5]}};
	}

	template <> DBox fromString(TextParser &parser) {
		double out[6];
		parser.parseDoubles(out);
		return {{out[0], out[1], out[2]}, {out[3], out[4], out[5]}};
	}

	template <> Matrix4 fromString(TextParser &parser) {
		float out[16];
		parser.parseFloats(out);
		return Matrix4(
			float4(out[0], out[1], out[2], out[3]), float4(out[4], out[5], out[6], out[7]),
			float4(out[8], out[9], out[10], out[11]), float4(out[12], out[13], out[14], out[15]));
	}

	template <> Quat fromString(TextParser &parser) {
		float out[4];
		parser.parseFloats(out);
		return Quat(out[0], out[1], out[2], out[3]);
	}

	template <> vector<string> vectorFromString<string>(TextParser &parser) {
		vector<string> out(parser.countElements());
		parser.parseStrings(out);
		return out;
	}

	template <> vector<float> vectorFromString<float>(TextParser &parser) {
		vector<float> out(parser.countElements());
		parser.parseFloats(out);
		return out;
	}

	template <> vector<double> vectorFromString<double>(TextParser &parser) {
		vector<double> out(parser.countElements());
		parser.parseDoubles(out);
		return out;
	}

	template <> vector<int> vectorFromString<int>(TextParser &parser) {
		vector<int> out(parser.countElements());
		parser.parseInts(out);
		return out;
	}

	template <> vector<uint> vectorFromString<uint>(TextParser &parser) {
		vector<uint> out(parser.countElements());
		parser.parseUints(out);
		return out;
	}
}
}
