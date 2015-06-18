/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_base.h"
#include <cstdio>
#include <stdarg.h>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <algorithm>
#include <functional>

namespace fwk {

/*
int TextParser::operator()(const char *format, ...) {
va_list args;
va_start(args, format);
int ret = vsscanf(m_current, format, args);
va_end(args);
return ret;
}*/

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

template <class T>
static int parseMultiple(const char *input, Range<T> out, const char *format,
						 const char *type_name) {
	int num_parsed = 0, count = out.size();
	const char *current = input;

	while(num_parsed + 4 < count) {
		int offset = 0;
		auto *tout = &out[num_parsed];
		if(sscanf(current, format, tout + 0, tout + 1, tout + 2, tout + 3, &offset) != 4)
			goto ERROR;
		num_parsed += 4;
		current += offset;
	}

	format += 9;
	while(num_parsed < count) {
		int offset = 0;
		if(sscanf(current, format, &out[num_parsed], &offset) != 1)
			goto ERROR;
		num_parsed++;
		current += offset;
	}

	return current - input;
ERROR : {
	string what = out.size() > 1 ? fwk::format("%d %s", out.size(), type_name) : type_name;
	THROW("Error while parsing %s from \"%s\"", what.c_str(), input);
}
	return 0;
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

void TextParser::parseInts(Range<int> out) {
	m_current += parseMultiple(m_current, out, " %d %d %d %d%n", "int");
}

void TextParser::parseFloats(Range<float> out) {
	m_current += parseMultiple(m_current, out, " %f %f %f %f%n", "float");
}

void TextParser::parseUints(Range<uint> out) {
	m_current += parseMultiple(m_current, out, " %u %u %u %u%n", "uint");
}

void TextParser::parseStrings(Range<string> out) {
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

	if(count < out.size()) {
		string what = out.size() > 1 ? fwk::format("%d strings", out.size()) : "strings";
		THROW("Error while parsing %s from \"%s\"", what.c_str(), m_current);
	}
	m_current = iptr;
}

bool TextParser::hasAnythingLeft() {
	const char *ptr = m_current;
	while(isspace(*ptr))
		ptr++;
	m_current = ptr;
	return !isFinished();
}
}
