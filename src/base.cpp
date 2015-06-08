/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_base.h"
#include <cstdio>

#ifdef FWK_TARGET_MINGW
#include <ctime>
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

#ifdef FWK_TARGET_LINUX
#include <execinfo.h>
#endif
#include <stdarg.h>
#include <cstring>
#include <cstdlib>
#include <cstdarg>

namespace fwk {

void sleep(double sec) {
	if(sec <= 0)
		return;
#ifdef _WIN32
	// TODO: check accuracy
	SleepEx(int(sec * 1e3), true);
#else
	usleep(int(sec * 1e6));
#endif
}

double getTime() {
#ifdef _WIN32
	__int64 tmp;
	QueryPerformanceFrequency((LARGE_INTEGER *)&tmp);
	double freq = double(tmp == 0 ? 1 : tmp);
	if(QueryPerformanceCounter((LARGE_INTEGER *)&tmp))
		return double(tmp) / freq;

	clock_t c = clock();
	return double(c) / double(CLOCKS_PER_SEC);
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_nsec / 1.0e9 + ts.tv_sec;
#endif
}

const string backtrace(size_t skip) {
	string out;
#ifndef FWK_TARGET_LINUX
	// ThrowException("write me");
	out = "Backtraces in LibFWK are supported only on Linux";
#else
	void *array[32];
	size_t size = ::backtrace(array, sizeof(array) / sizeof(void *));
	char **strings = backtrace_symbols(array, size);
	char buffer[1024], *ptr = buffer;
	int buf_size = (int)sizeof(buffer);

	for(size_t i = skip; i < size; i++) {
		int len = strlen(strings[i]);
		int max_len = len < buf_size ? len : buf_size;
		memcpy(ptr, strings[i], max_len);
		ptr += max_len;
		buf_size -= max_len;
		if(buf_size) {
			*ptr++ = '\n';
			buf_size--;
		}
	}
	ptr[buf_size ? 0 : -1] = 0;
	free(strings);
	out = buffer;
#endif

	return out;
}

static void filterString(string &str, const char *src, const char *dst) {
	DASSERT(src && dst);
	int src_len = strlen(src);

	auto pos = str.find(src);
	while(pos != std::string::npos) {
		str = str.substr(0, pos) + dst + str.substr(pos + src_len);
		pos = str.find(src);
	}
}

static const char *s_filtered_names[] = {
	"std::basic_string<char, std::char_traits<char>, std::allocator<char> >", "fwk::string",
};

const string cppFilterBacktrace(const string &input) {
#if !defined(_WIN32) && !defined(__MINGW32__)
	string command = "echo \"" + input + "\" | c++filt -n";

	FILE *file = popen(command.c_str(), "r");
	if(file) {
		vector<char> buf;
		try {
			while(!feof(file) && buf.size() < 1024 * 4) {
				buf.push_back(fgetc(file));
				if((u8)buf.back() == 255) {
					buf.pop_back();
					break;
				}
			}
		} catch(...) {
			fclose(file);
			throw;
		}
		fclose(file);

		string out(&buf[0], buf.size());
		for(int n = 0; n < (int)arraySize(s_filtered_names); n += 2)
			filterString(out, s_filtered_names[n], s_filtered_names[n + 1]);
		return out;
	}
#endif
	return input;
}

Exception::Exception(const char *str) : m_data(str) { m_backtrace = fwk::backtrace(3); }
Exception::Exception(const string &s) : m_data(s) { m_backtrace = fwk::backtrace(3); }
Exception::Exception(const string &s, const string &bt) : m_data(s), m_backtrace(bt) {}

void throwException(const char *file, int line, const char *fmt, ...) {
	char new_fmt[2048];
	snprintf(new_fmt, sizeof(new_fmt), "%s:%d: %s", file, line, fmt);

	// TODO: support custom types, like vector
	char buffer[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), new_fmt, ap);
	va_end(ap);

	throw Exception(buffer);
}

void doAssert(const char *file, int line, const char *text) {
	char new_text[2048];
	snprintf(new_text, sizeof(new_text), "%s:%d: Assertion failed: %s", file, line, text);
	throw fwk::Exception(new_text);
}

#ifdef _WIN32

const char *strcasestr(const char *a, const char *b) {
	DASSERT(a && b);

	while(*a) {
		if(strcasecmp(a, b) == 0)
			return a;
		a++;
	}

	return nullptr;
}

#endif

void logError(const string &error) { fprintf(stderr, "%s", error.c_str()); }

int enumFromString(const char *str, const char **strings, int count, bool handle_invalid) {
	DASSERT(str);
	for(int n = 0; n < count; n++)
		if(strcmp(str, strings[n]) == 0)
			return n;

	if(!handle_invalid) {
		char tstrings[1024], *ptr = tstrings;
		for(int i = 0; i < count; i++)
			ptr += snprintf(ptr, sizeof(tstrings) - (ptr - tstrings), "%s ", strings[i]);
		if(count)
			ptr[-1] = 0;
		THROW("Error while finding string \"%s\" in array (%s)", str, tstrings);
	}

	return -1;
}

BitVector::BitVector(int size) : m_data((size + base_size - 1) / base_size), m_size(size) {}

void BitVector::resize(int new_size, bool clear_value) {
	PodArray<u32> new_data(new_size);
	memcpy(new_data.data(), m_data.data(), sizeof(base_type) * min(new_size, m_data.size()));
	if(new_data.size() > m_data.size())
		memset(new_data.data() + m_data.size(), clear_value ? 0xff : 0,
			   (new_data.size() - m_data.size()) * sizeof(base_type));
	m_data.swap(new_data);
}

void BitVector::clear(bool value) {
	memset(m_data.data(), value ? 0xff : 0, m_data.size() * sizeof(base_type));
}

TextFormatter::TextFormatter(int size) : m_offset(0), m_data(size) {
	DASSERT(size > 0);
	m_data[0] = 0;
}

void TextFormatter::operator()(const char *format, ...) {
	bool realloc_needed = false;

	do {
		va_list ap;
		va_start(ap, format);
		int offset = vsnprintf(&m_data[m_offset], m_data.size() - m_offset, format, ap);
		va_end(ap);

		if(offset < 0) {
			m_data[m_offset] = 0;
			THROW("Textformatter: error while encoding");
		}

		if(m_offset + offset >= (int)m_data.size()) {
			m_data[m_offset] = 0;
			PodArray<char> new_data((m_offset + offset + 1) * (m_offset == 0 ? 1 : 2));
			memcpy(new_data.data(), m_data.data(), m_offset + 1);
			new_data.swap(m_data);
			realloc_needed = true;
		} else {
			m_offset += offset;
			realloc_needed = false;
		}
	} while(realloc_needed);
}

string format(const char *format, ...) {
	char buffer[4096];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	return string(buffer);
}
}
