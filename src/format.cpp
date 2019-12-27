// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/format.h"

#include "fwk/math/box.h"
#include <cfloat>
#include <cstring>
#include <stdarg.h>

namespace fwk {

namespace detail {
	string autoPrintFormat(const char *args) {
		TextFormatter out;
		// TODO: properly handle parenthesis, etc.
		Tokenizer tok(args, ',');

		auto append_elem = [&](Str elem) {
			for(auto c : elem)
				if(c == '%')
					out << "\\%";
				else
					out << c;
		};

		bool back_trim = false;
		while(!tok.finished()) {
			auto elem = tok.next();

			while(elem && isspace(elem[0]))
				elem = elem.substr(1);
			while(elem && isspace(elem[elem.size() - 1]))
				elem = elem.substr(0, elem.size() - 1);

			// TODO: handle % in elems
			DASSERT(elem);
			back_trim = true;
			if(elem[0] == '"' && elem[elem.size() - 1] == '"') {
				out << "%";
				back_trim = false;
			} else if(anyOf(elem, isspace)) {
				out << '"';
				append_elem(elem);
				out << "\":% ";
			} else {
				append_elem(elem);
				out << ":% ";
			}
		}

		if(back_trim)
			out.trim(1);
		out << '\n';

		return out.text();
	}
}

TextFormatter::TextFormatter(int capacity, FormatOptions options)
	: m_data(capacity), m_offset(0), m_options(options) {
	DASSERT(capacity > 0);
	m_data[0] = 0;
}

TextFormatter::TextFormatter(const TextFormatter &) = default;
TextFormatter::~TextFormatter() = default;

void TextFormatter::reserve(int capacity) {
	if(capacity > m_data.capacity())
		m_data.resize(vectorInsertCapacity(m_data.size(), 1, capacity));
}

void TextFormatter::clear() {
	m_data[0] = 0;
	m_offset = 0;
}

static int countPercents(const char *ptr) {
	int out = 0;
	char prev = 0;
	while(*ptr) {
		if(*ptr == '%' && prev != '\\')
			out++;
		prev = *ptr++;
	}
	return out;
}

void TextFormatter::append_(const char *format_str, int arg_count, const Func *funcs, va_list ap) {
	static const Func opt_funcs[] = {detail::append<FormatOptions>, detail::append<FormatMode>,
									 detail::append<FormatPrecision>};

#ifndef NDEBUG
	DASSERT(format_str);

	int num_percent = countPercents(format_str);
	int num_format_args = 0;
	for(int n = 0; n < arg_count; n++)
		if(isOneOf(funcs[n], opt_funcs))
			num_format_args++;
	if(num_percent != arg_count - num_format_args)
		FATAL("Invalid nr of arguments passed: %d, expected: %d\nFormat string: \"%s\"",
			  arg_count - num_format_args, num_percent, format_str);
#endif

	for(int n = 0; n < arg_count; n++) {
		auto arg = va_arg(ap, unsigned long long);
		if(!isOneOf(funcs[n], opt_funcs))
			format_str = nextElement(format_str);
		funcs[n](*this, arg);
	}

	while(*format_str) {
		if(!(format_str[0] == '\\' && format_str[1] == '%'))
			*this << *format_str;
		format_str++;
	}
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

TextFormatter &TextFormatter::operator<<(Str text) {
	if(text.empty())
		return *this;

	int new_size = m_offset + text.size() + 1;
	if(new_size > m_data.size())
		reserve(new_size);

	memcpy(&m_data[m_offset], text.data(), text.size());
	m_offset += text.size();
	m_data[m_offset] = 0;

	return *this;
}

TextFormatter &TextFormatter::operator<<(const char *str) { return *this << Str(str); }
TextFormatter &TextFormatter::operator<<(const string &str) { return *this << Str(str); }

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
	PASSERT(count > 0 && count <= m_offset);
	m_offset = max(0, m_offset - count);
	m_data[m_offset] = 0;
}

const char *TextFormatter::nextElement(const char *format_str) {
	const char *start = format_str;
	char prev = 0;
	while(*format_str && *format_str != '%')
		prev = *format_str++;

	if(prev == '\\' && *format_str == '%') {
		*this << Str(start, format_str - 1) << '%';
		return nextElement(format_str + 1);
	}

	const char *end = format_str++;

	*this << Str(start, end);
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
	PASS_POINTER(Str)

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

template TextFormatter &operator<<(TextFormatter &, const vec2<int> &);
template TextFormatter &operator<<(TextFormatter &, const vec3<int> &);
template TextFormatter &operator<<(TextFormatter &, const vec4<int> &);

template TextFormatter &operator<<(TextFormatter &, const vec2<float> &);
template TextFormatter &operator<<(TextFormatter &, const vec3<float> &);
template TextFormatter &operator<<(TextFormatter &, const vec4<float> &);

template TextFormatter &operator<<(TextFormatter &, const vec2<double> &);
template TextFormatter &operator<<(TextFormatter &, const vec3<double> &);
template TextFormatter &operator<<(TextFormatter &, const vec4<double> &);

template TextFormatter &operator<<(TextFormatter &, const Box<int2> &);
template TextFormatter &operator<<(TextFormatter &, const Box<int3> &);
template TextFormatter &operator<<(TextFormatter &, const Box<float2> &);
template TextFormatter &operator<<(TextFormatter &, const Box<float3> &);
template TextFormatter &operator<<(TextFormatter &, const Box<double2> &);
template TextFormatter &operator<<(TextFormatter &, const Box<double3> &);

TextFormatter &operator<<(TextFormatter &fmt, const None &) {
	fmt << "none";
	return fmt;
}

namespace detail {
	void formatSpan(TextFormatter &out, const char *data, int size, int obj_size, TFFunc func) {
		const char *separator = out.isStructured() ? ", " : " ";

		if(out.isStructured())
			out << '[';
		for(int n = 0; n < size; n++) {
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
