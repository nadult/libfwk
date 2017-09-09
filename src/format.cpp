// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_format.h"

#include "fwk_math_ext.h"
#include <cfloat>
#include <cstring>
#include <stdarg.h>

namespace fwk {

TextFormatter::TextFormatter(int size, FormatOptions options)
	: m_data(size), m_offset(0), m_options(options) {
	DASSERT(size > 0);
	m_data[0] = 0;
}

TextFormatter::TextFormatter(const TextFormatter &) = default;
TextFormatter::~TextFormatter() = default;

void TextFormatter::reserve(int capacity) {
	if(capacity <= m_data.size())
		return;

	PodArray<char> new_data(BaseVector::insertCapacity(m_data.size(), 1, capacity));
	memcpy(new_data.data(), m_data.data(), m_offset + 1);
	new_data.swap(m_data);
}

void TextFormatter::stdFormat(const char *format, ...) {
	while(true) {
		va_list ap;
		va_start(ap, format);
		int ret = vsnprintf(&m_data[m_offset], m_data.size() - m_offset, format, ap);
		va_end(ap);

		if(ret < 0)
			FATAL("Error in vsnprintf(\"%s\"): errno: %s(%d)", format, strerror(errno), errno);

		int new_offset = m_offset + ret;
		if(new_offset + 1 <= m_data.size()) {
			m_offset = new_offset;
			return;
		}

		reserve(new_offset + 1);
	}
}

#define EXPECT_TAKEN(a) __builtin_expect(!!(a), 1)

void TextFormatter::append(StringRef text) {
	if(text.empty())
		return;

	int new_size = m_offset + text.size() + 1;
	if(new_size > m_data.size())
		reserve(new_size);

	memcpy(&m_data[m_offset], text.c_str(), text.size());
	m_offset += text.size();
	m_data[m_offset] = 0;
}

void TextFormatter::append(char c) {
	char buf[2] = {c, 0};
	append(buf);
}

void TextFormatter::append(double value) {
	stdFormat(m_options.precision == FormatPrecision::maximum ? "%.17f" : "%f", value);

	int pos = m_offset - 1;
	while(pos >= 0 && m_data[pos] == '0')
		pos--;
	if(pos >= 0 && m_data[pos] == '.')
		pos--;
	m_data[m_offset = pos + 1] = 0;
}

void TextFormatter::append(float value) {
	stdFormat(m_options.precision == FormatPrecision::maximum ? "%.9f" : "%f", value);

	int pos = m_offset - 1;
	while(pos >= 0 && m_data[pos] == '0')
		pos--;
	if(pos >= 0 && m_data[pos] == '.')
		pos--;
	m_data[m_offset = pos + 1] = 0;
}

void TextFormatter::append(int value) { stdFormat("%d", value); }
void TextFormatter::append(uint value) { stdFormat("%u", value); }
void TextFormatter::append(long value) { stdFormat("%ld", value); }
void TextFormatter::append(unsigned long value) { stdFormat("%lu", value); }
void TextFormatter::append(long long value) { stdFormat("%lld", value); }
void TextFormatter::append(unsigned long long value) { stdFormat("%llu", value); }
void TextFormatter::append(bool value) { append(value ? "true" : "false"); }

void TextFormatter::trim(int count) {
	PASSERT(count > m_offset);
	m_offset = max(0, m_offset - count);
	m_data[m_offset] = 0;
}

const char *TextFormatter::nextElement(const char *format_str) {
	const char *start = format_str;
	while(*format_str && *format_str != '%')
		format_str++;
	const char *end = format_str++;

	append(StringRef(start, (int)(end - start)));
	return format_str;
}

void TextFormatter::checkArgumentCount(const char *str, int num_arg) {
	int num_percent = std::count(str, str + strlen(str), '%');
	if(num_percent != num_arg)
		FATAL("Invalid number of arguments; passed: %d excluding formatting options; expected: "
			  "%d; Format string: \"%s\"",
			  num_arg, num_percent, str);
}

string stdFormat(const char *format, ...) {
	char buffer[4096];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	return string(buffer);
}

void format(TextFormatter &out, const char *value) { out.append(value); }
void format(TextFormatter &out, StringRef value) { out.append(value); }
void format(TextFormatter &out, const string &value) { out.append(value); }

void format(TextFormatter &out, bool value) { out.append(value); }
void format(TextFormatter &out, int value) { out.append(value); }
void format(TextFormatter &out, uint value) { out.append(value); }
void format(TextFormatter &out, unsigned long value) { out.append(value); }
void format(TextFormatter &out, long long value) { out.append(value); }
void format(TextFormatter &out, unsigned long long value) { out.append(value); }
void format(TextFormatter &out, double value) { out.append(value); }
void format(TextFormatter &out, float value) { out.append(value); }

void format(TextFormatter &out, const int2 &value) {
	out.stdFormat(out.options().mode == FormatMode::plain ? "%d %d" : "(%d, %d)", value.x, value.y);
}
void format(TextFormatter &out, const int3 &value) {
	out.stdFormat(out.options().mode == FormatMode::plain ? "%d %d %d" : "(%d, %d, %d)", value.x,
				  value.y, value.z);
}
void format(TextFormatter &out, const int4 &value) {
	out.stdFormat(out.options().mode == FormatMode::plain ? "%d %d %d %d" : "(%d, %d, %d, %d)",
				  value.x, value.y, value.z, value.w);
}

void format(TextFormatter &out, const float2 &value) { format<float2>(out, value); }
void format(TextFormatter &out, const float3 &value) { format<float3>(out, value); }
void format(TextFormatter &out, const float4 &value) { format<float4>(out, value); }
void format(TextFormatter &out, const double2 &value) { format<double2>(out, value); }
void format(TextFormatter &out, const double3 &value) { format<double3>(out, value); }
void format(TextFormatter &out, const double4 &value) { format<double4>(out, value); }

void format(TextFormatter &out, const FRect &value) { format<float2>(out, value); }
void format(TextFormatter &out, const DRect &value) { format<double2>(out, value); }
void format(TextFormatter &out, const IRect &value) { format<int2>(out, value); }
void format(TextFormatter &out, const FBox &value) { format<float3>(out, value); }
void format(TextFormatter &out, const DBox &value) { format<double3>(out, value); }
void format(TextFormatter &out, const IBox &value) { format<int3>(out, value); }

void format(TextFormatter &out, const Matrix4 &matrix) {
	out(out.isStructured() ? "(%; %; %; %)" : "% % % %", matrix[0], matrix[1], matrix[2],
		matrix[3]);
}

void format(TextFormatter &out, const Quat &value) { format<float4>(out, float4(value)); }

void format(TextFormatter &out, qint value) {
	// Max digits: about 36
	char buffer[64];
	int pos = 0;

	bool sign = value < 0;
	if(value < 0)
		value = -value;

	while(value > 0) {
		int digit(value % 10);
		value /= 10;
		buffer[pos++] = '0' + digit;
	}

	if(pos == 0)
		buffer[pos++] = '0';
	if(sign)
		buffer[pos++] = '-';
	buffer[pos] = 0;

	std::reverse(buffer, buffer + pos);
	out.append(buffer);
}
}
