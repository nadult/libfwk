// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/format.h"

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
	m_data.resize(BaseVector::insertCapacity(m_data.size(), 1, capacity));
}

void TextFormatter::append_(const char *format_str, int arg_count, const Func *funcs, va_list ap) {
	static const Func opt_funcs[] = {detail::append<FormatOptions>, detail::append<FormatMode>,
									 detail::append<FormatPrecision>};

#ifndef NDEBUG
	DASSERT(format_str);

	int num_percent = std::count(format_str, format_str + strlen(format_str), '%');
	int num_format_args = 0;
	for(int n = 0; n < arg_count; n++)
		if(isOneOf(funcs[n], opt_funcs))
			num_format_args++;
	DASSERT(num_percent == arg_count - num_format_args && "Invalid nr of arguments passed");
#endif

	for(int n = 0; n < arg_count; n++) {
		auto arg = va_arg(ap, unsigned long long);
		if(!isOneOf(funcs[n], opt_funcs))
			format_str = nextElement(format_str);
		funcs[n](*this, arg);
	}
	*this << format_str;
}

void TextFormatter::append_(const char *format, int arg_count, const Func *funcs, ...) {
	va_list ap;
	va_start(ap, funcs);
	append_(format, arg_count, funcs, ap);
	va_end(ap);
}

string TextFormatter::strFormat_(const char *format, int arg_count, const Func *funcs, ...) {
	TextFormatter out;
	va_list ap;
	va_start(ap, funcs);
	out.append_(format, arg_count, funcs, ap);
	va_end(ap);
	return out.text();
}

void TextFormatter::print_(FormatMode mode, const char *format, int arg_count, const Func *funcs,
						   ...) {
	TextFormatter out(1024, {mode});
	va_list ap;
	va_start(ap, funcs);
	out.append_(format, arg_count, funcs, ap);
	va_end(ap);
	fputs(out.c_str(), stdout);
}

string TextFormatter::toString_(Func func, Value value) {
	TextFormatter out;
	func(out, value);
	return out.text();
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

TextFormatter &TextFormatter::operator<<(StringRef text) {
	if(text.empty())
		return *this;

	int new_size = m_offset + text.size() + 1;
	if(new_size > m_data.size())
		reserve(new_size);

	memcpy(&m_data[m_offset], text.c_str(), text.size());
	m_offset += text.size();
	m_data[m_offset] = 0;

	return *this;
}

TextFormatter &TextFormatter::operator<<(const char *str) { return *this << StringRef(str); }
TextFormatter &TextFormatter::operator<<(const string &str) { return *this << StringRef(str); }

TextFormatter &TextFormatter::operator<<(char c) {
	if(c) {
		char buf[2] = {c, 0};
		*this << buf;
	}
	return *this;
}

TextFormatter &TextFormatter::operator<<(double value) {
	stdFormat(m_options.precision == FormatPrecision::maximum ? "%.17f" : "%f", value);

	int pos = m_offset - 1;
	while(pos >= 0 && m_data[pos] == '0')
		pos--;
	if(pos >= 0 && m_data[pos] == '.')
		pos--;
	m_data[m_offset = pos + 1] = 0;
	return *this;
}

TextFormatter &TextFormatter::operator<<(float value) {
	stdFormat(m_options.precision == FormatPrecision::maximum ? "%.9f" : "%f", value);

	int pos = m_offset - 1;
	while(pos >= 0 && m_data[pos] == '0')
		pos--;
	if(pos >= 0 && m_data[pos] == '.')
		pos--;
	m_data[m_offset = pos + 1] = 0;
	return *this;
}

TextFormatter &TextFormatter::operator<<(int value) {
	stdFormat("%d", value);
	return *this;
}
TextFormatter &TextFormatter::operator<<(unsigned int value) {
	stdFormat("%u", value);
	return *this;
}
TextFormatter &TextFormatter::operator<<(long value) {
	stdFormat("%ld", value);
	return *this;
}
TextFormatter &TextFormatter::operator<<(unsigned long value) {
	stdFormat("%lu", value);
	return *this;
}
TextFormatter &TextFormatter::operator<<(long long value) {
	stdFormat("%lld", value);
	return *this;
}
TextFormatter &TextFormatter::operator<<(unsigned long long value) {
	stdFormat("%llu", value);
	return *this;
}
TextFormatter &TextFormatter::operator<<(bool value) { return *this << (value ? "true" : "false"); }

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

	*this << StringRef(start, (int)(end - start));
	return format_str;
}

string stdFormat(const char *format, ...) {
	char buffer[4096];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	return string(buffer);
}

namespace detail {
#define PASS_VALUE(type)                                                                           \
	template <> void append<type>(TextFormatter & fmt, TFValue val) { fmt << (type)val; }
#define PASS_POINTER(type)                                                                         \
	template <> void append<type>(TextFormatter & fmt, TFValue val) { fmt << *(const type *)val; }

#define PASS_FORMAT_OPTS(type)                                                                     \
	template <> void append<type>(TextFormatter & fmt, TFValue val) {                              \
		fmt.options().set(*(const type *)val);                                                     \
	}

	PASS_VALUE(char)
	PASS_VALUE(unsigned char)
	PASS_VALUE(short)
	PASS_VALUE(unsigned short)
	PASS_VALUE(int)
	PASS_VALUE(unsigned int)
	PASS_VALUE(long)
	PASS_VALUE(unsigned long)
	PASS_VALUE(long long)
	PASS_VALUE(unsigned long long)
	PASS_VALUE(bool)

	PASS_POINTER(double)
	PASS_POINTER(float)

	PASS_FORMAT_OPTS(FormatOptions)
	PASS_FORMAT_OPTS(FormatMode)
	PASS_FORMAT_OPTS(FormatPrecision)

#undef PASS_VALUE
#undef PASS_POINTER
#undef PASS_FORMAT_OPTS

	template <> void append<const char *>(TextFormatter &fmt, TFValue val) {
		fmt << ((const char *)val);
	}
}

TextFormatter &operator<<(TextFormatter &out, const int2 &value) {
	out.stdFormat(out.options().mode == FormatMode::plain ? "%d %d" : "(%d, %d)", value.x, value.y);
	return out;
}
TextFormatter &operator<<(TextFormatter &out, const int3 &value) {
	out.stdFormat(out.options().mode == FormatMode::plain ? "%d %d %d" : "(%d, %d, %d)", value.x,
				  value.y, value.z);
	return out;
}
TextFormatter &operator<<(TextFormatter &out, const int4 &value) {
	out.stdFormat(out.options().mode == FormatMode::plain ? "%d %d %d %d" : "(%d, %d, %d, %d)",
				  value.x, value.y, value.z, value.w);
	return out;
}

TextFormatter &operator<<(TextFormatter &out, const float2 &value) {
	return operator<<<float2>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const float3 &value) {
	return operator<<<float3>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const float4 &value) {
	return operator<<<float4>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const double2 &value) {
	return operator<<<double2>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const double3 &value) {
	return operator<<<double3>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const double4 &value) {
	return operator<<<double4>(out, value);
}

TextFormatter &operator<<(TextFormatter &out, const FRect &value) {
	return operator<<<float2>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const DRect &value) {
	return operator<<<double2>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const IRect &value) {
	return operator<<<int2>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const FBox &value) {
	return operator<<<float3>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const DBox &value) {
	return operator<<<double3>(out, value);
}
TextFormatter &operator<<(TextFormatter &out, const IBox &value) {
	return operator<<<int3>(out, value);
}

TextFormatter &operator<<(TextFormatter &out, const Matrix4 &matrix) {
	out(out.isStructured() ? "(%; %; %; %)" : "% % % %", matrix[0], matrix[1], matrix[2],
		matrix[3]);
	return out;
}

TextFormatter &operator<<(TextFormatter &out, const Quat &value) {
	return operator<<<float4>(out, float4(value));
}

TextFormatter &operator<<(TextFormatter &out, qint value) {
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
	out << buffer;
	return out;
}

namespace detail {
	void formatSpan(TextFormatter &out, const char *data, int size, int obj_size, TFFunc func) {
		const char *separator = out.isStructured() ? ", " : " ";

		if(out.isStructured())
			out << '[';
		for(int n : intRange(size)) {
			auto ptr = data + size_t(n) * obj_size;
			func(out, (unsigned long long)ptr);
			if(n + 1 != size)
				out << separator;
		}
		if(out.isStructured())
			out << ']';
	}
}
}
