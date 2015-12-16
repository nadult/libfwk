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

#ifdef FWK_TARGET_HTML5
#include "emscripten.h"
#endif

namespace fwk {

void *SimpleAllocatorBase::allocateBytes(size_t count) noexcept {
	void *out = malloc(count);
	if(!out) {
		auto bt = Backtrace::get(1);
		printf("Fatal error while allocating memory (%llu bytes)\n", (unsigned long long)count);
		printf("Generating backtrace:\n");
		try {
			string text = bt.analyze(false);
			printf("%s\n", text.c_str());
		} catch(const Exception &ex) { printf("Failed:\n%s\n", ex.what()); }
		exit(1);
	}

	return out;
}

wstring toWideString(StringRef text, bool throw_on_invalid) {
	mbstate_t ps;
	memset(&ps, 0, sizeof(ps));
	const char *str = text.c_str();
	PodArray<wchar_t> buffer(text.size());

	auto size = mbsrtowcs(buffer.data(), &str, buffer.size(), &ps);
	if(size == (size_t)-1) {
		if(throw_on_invalid)
			THROW("Error when converting string to wide string");
		return wstring();
	}
	return wstring(buffer.data(), buffer.data() + size);
}

string fromWideString(const std::wstring &text, bool throw_on_invalid) {
	PodArray<char> buffer(text.size() * 4);
	mbstate_t ps;
	memset(&ps, 0, sizeof(ps));

	const wchar_t *src = text.c_str();
	auto size = wcsrtombs(buffer.data(), &src, buffer.size(), &ps);
	if(size == (size_t)-1) {
		if(throw_on_invalid)
			THROW("Error while converting wide string to string");
		return string();
	}
	return string(buffer.data(), buffer.data() + size);
}

#ifndef FWK_TARGET_HTML5

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
		printf("Backtrace:\n%s\n", Backtrace::get(2).analyze(true).c_str());
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

#endif

// TODO: stdout and stderr returned separately?
pair<string, bool> execCommand(const string &cmd) {
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
	return make_pair(result, ret == 0);
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

Exception::Exception(string text) : m_text(std::move(text)), m_backtrace(Backtrace::get(3)) {}
Exception::Exception(string text, Backtrace bt)
	: m_text(std::move(text)), m_backtrace(std::move(bt)) {}

void throwException(const char *file, int line, const char *fmt, ...) {
	char new_fmt[2048];
	snprintf(new_fmt, sizeof(new_fmt), "%s:%d: %s", file, line, fmt);

	// TODO: support custom types, like vector
	char buffer[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), new_fmt, ap);
	va_end(ap);

#ifdef FWK_TARGET_HTML5
	printf("Exception thrown: %s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "Exception thrown: %s\n", buffer);
	emscripten_force_exit(1);
#else
	throw Exception(buffer);
#endif
}

void doAssert(const char *file, int line, const char *text) {
	char buffer[2048];
	snprintf(buffer, sizeof(buffer), "%s:%d: Assertion failed: %s", file, line, text);
#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	throw fwk::Exception(buffer);
#endif
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
		TextFormatter all_strings;
		for(int i = 0; i < count; i++)
			all_strings("%s%s", strings[i], i + 1 < count ? " " : "");
		THROW("Error when parsing enum: couldn't match \"%s\" to (%s)", str, all_strings.text());
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

string simpleFormat(const char *format, const vector<string> &args) {
	TextFormatter out;

	DASSERT(std::count(format, format + strlen(format), '%') == (int)args.size());
	size_t arg_id = 0;
	for(const char *c = format; *c; c++) {
		if(*c == '%') {
			out("%s", arg_id >= args.size() ? "" : args[arg_id].c_str());
			arg_id++;
		} else { out("%c", *c); }
	}

	return out.text();
}
}
