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
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef FWK_TARGET_LINUX
#include <execinfo.h>
#include <dlfcn.h>
#endif
#include <stdarg.h>
#include <cstring>
#include <cstdlib>
#include <cstdarg>

namespace fwk {

namespace {

	struct AutoSegHandler {
		AutoSegHandler() { handleSegFault(); }
	} s_auto_seg_handler;

	void (*s_handler)() = 0;

#ifdef _WIN32
	BOOL shandler(DWORD type) {
		if(type == CTRL_C_EVENT && s_handler) {
			s_handler();
			return TRUE;
		}

		return FALSE;
	}
#else
	void shandler(int) {
		if(s_handler)
			s_handler();
	}
#endif

#ifndef _WIN32
	void segfaultHandler(int, siginfo_t *, void *) {
		printf("Segmentation fault!\n");
		printf("Backtrace:\n%s\n", cppFilterBacktrace(backtrace(2)).c_str());
		exit(1);
	}
#endif
}

void handleCtrlC(void (*handler)()) {
	s_handler = handler;
#ifdef _WIN32
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)shandler, TRUE);
#else
	struct sigaction sig_int_handler;

	sig_int_handler.sa_handler = shandler;
	sigemptyset(&sig_int_handler.sa_mask);
	sig_int_handler.sa_flags = 0;
	sigaction(SIGINT, &sig_int_handler, NULL);
#endif
}

void handleSegFault() {
#ifdef _WIN32
// TODO: writeme
#else
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = segfaultHandler;
	if(sigaction(SIGSEGV, &sa, NULL) == -1)
		THROW("Error while attaching segfault handler");
#endif
}

// TODO: stdout and stderr returned separately?
string execCommand(const string &cmd) {
	FILE *pipe = popen(cmd.c_str(), "r");
	if(!pipe)
		THROW("error while executing command: '%s'", cmd.c_str());
	char buffer[1024];
	std::string result = "";
	while(!feof(pipe)) {
		if(fgets(buffer, sizeof(buffer), pipe))
			result += buffer;
	}
	int ret = pclose(pipe);
	if(ret != 0) // TODO: return two arguments (result and error code)?
		THROW("Error while executing command:\n\"%s\"\nResult: %s\n", cmd.c_str(), result.c_str());
	return result;
}

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

int enumFromString(const char *str, const char **strings, int count, bool throw_on_invalid) {
	DASSERT(str);
	for(int n = 0; n < count; n++)
		if(strcmp(str, strings[n]) == 0)
			return n;

	if(throw_on_invalid) {
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

string format(const char *format, ...) {
	char buffer[4096];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	return string(buffer);
}
}
