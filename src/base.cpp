/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_base.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdarg.h>

#if defined(FWK_TARGET_MINGW) || defined(FWK_TARGET_MSVC)
#include <ctime>
#include <windows.h>
#else
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif

#ifdef FWK_TARGET_LINUX
#include <dlfcn.h>
#include <execinfo.h>
#endif

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
		} catch(const Exception &ex) {
			printf("Failed:\n%s\n", ex.what());
		}
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

namespace {

	void (*s_user_ctrlc_func)() = 0;

#if defined(FWK_TARGET_MINGW)

	BOOL handleControlC(DWORD type) {
		if(type == CTRL_C_EVENT && s_user_ctrlc_func) {
			s_user_ctrlc_func();
			return TRUE;
		}

		return FALSE;
	}

	const char *exceptionName(DWORD code) {
		switch(code) {
#define CASE(code)                                                                                 \
	case code:                                                                                     \
		return #code;
			CASE(EXCEPTION_ACCESS_VIOLATION)
			CASE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
			CASE(EXCEPTION_BREAKPOINT)
			CASE(EXCEPTION_DATATYPE_MISALIGNMENT)
			CASE(EXCEPTION_FLT_DENORMAL_OPERAND)
			CASE(EXCEPTION_FLT_DIVIDE_BY_ZERO)
			CASE(EXCEPTION_FLT_INEXACT_RESULT)
			CASE(EXCEPTION_FLT_INVALID_OPERATION)
			CASE(EXCEPTION_FLT_OVERFLOW)
			CASE(EXCEPTION_FLT_STACK_CHECK)
			CASE(EXCEPTION_FLT_UNDERFLOW)
			CASE(EXCEPTION_ILLEGAL_INSTRUCTION)
			CASE(EXCEPTION_IN_PAGE_ERROR)
			CASE(EXCEPTION_INT_DIVIDE_BY_ZERO)
			CASE(EXCEPTION_INT_OVERFLOW)
			CASE(EXCEPTION_INVALID_DISPOSITION)
			CASE(EXCEPTION_NONCONTINUABLE_EXCEPTION)
			CASE(EXCEPTION_PRIV_INSTRUCTION)
			CASE(EXCEPTION_SINGLE_STEP)
			CASE(EXCEPTION_STACK_OVERFLOW)
#undef CASE
		default:
			return "UNKNOWN";
		}
	}

	LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS *info) {
		printf("Signal received: %s\n", exceptionName(info->ExceptionRecord->ExceptionCode));
		printf("Backtrace:\n%s\n", Backtrace::get(2, info->ContextRecord).analyze(false).c_str());
		return EXCEPTION_EXECUTE_HANDLER;
	}

	struct AutoSigHandler {
		AutoSigHandler() { SetUnhandledExceptionFilter(windows_exception_handler); }
	} s_auto_sig_handler;

#elif defined(FWK_TARGET_LINUX)

	void handleControlC(int) {
		if(s_user_ctrlc_func)
			s_user_ctrlc_func();
	}

	void segfaultHandler(int, siginfo_t *, void *) {
		printf("Segmentation fault!\n");
		printf("Backtrace:\n%s\n", Backtrace::get(2).analyze(true).c_str());
		exit(1);
	}

	struct AutoSigHandler {
		AutoSigHandler() {
			struct sigaction sa;
			sa.sa_flags = SA_SIGINFO;
			sigemptyset(&sa.sa_mask);
			sa.sa_sigaction = segfaultHandler;
			if(sigaction(SIGSEGV, &sa, NULL) == -1)
				FATAL("Error while attaching segfault handler");
		}
	} s_auto_sig_handler;

#endif
}

void handleCtrlC(void (*handler)()) {
	s_user_ctrlc_func = handler;
#if defined(FWK_TARGET_MINGW)
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)handleControlC, TRUE);
#elif defined(FWK_TARGET_LINUX)
	struct sigaction sig_int_handler;

	sig_int_handler.sa_handler = handleControlC;
	sigemptyset(&sig_int_handler.sa_mask);
	sig_int_handler.sa_flags = 0;
	sigaction(SIGINT, &sig_int_handler, NULL);
#endif
}

#ifdef FWK_TARGET_MSVC
#define popen _popen
#define pclose _pclose
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

Exception::Exception(string text) : m_text(move(text)), m_backtrace(Backtrace::get(3)) {}
Exception::Exception(string text, Backtrace bt) : m_text(move(text)), m_backtrace(move(bt)) {}

const char *Exception::what() const noexcept {
	static char thread_local buffer[4096];

	try {
		TextFormatter fmt;
		fmt("%s\nBacktrace:\n%s", text(), backtrace().c_str());
		int len = min((int)strlen(fmt.text()), (int)arraySize(buffer) - 1);
		memcpy(buffer, fmt.text(), len);
		buffer[len] = 0;
	} catch(...) {
		return text();
	}
	return buffer;
}

void throwException(const char *file, int line, const char *fmt, ...) {
	char new_fmt[1024];
	snprintf(new_fmt, sizeof(new_fmt), "%s:%d: %s", file, line, fmt);

	char buffer[4096];
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

void fatalError(const char *file, int line, const char *fmt, ...) {
	char new_fmt[1024];
	snprintf(new_fmt, sizeof(new_fmt), "%s:%d: %s", file, line, fmt);

	char buffer[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), new_fmt, ap);
	va_end(ap);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	auto bt = Backtrace::get(1).analyze(true);
	printf("%s\nBacktrace:\n%s", buffer, bt.c_str());
#endif
	exit(1);
}

void assertFailed(const char *file, int line, const char *text) {
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "%s:%d: Assertion failed: %s", file, line, text);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	auto bt = Backtrace::get(1).analyze(true);
	printf("%s\nBacktrace:\n%s", buffer, bt.c_str());

	try {
		throw "Debugger can catch this";
	} catch(...) {
	}
#endif
	exit(1);
}

void checkFailed(const char *file, int line, const char *text) {
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "%s:%d: Check failed: %s", file, line, text);
#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	throw fwk::Exception(buffer, Backtrace::get(1));
#endif
}

#if defined(FWK_TARGET_MINGW) || defined(FWK_TARGET_MSVC)

static int strcasecmp(const char *a, const char *b) { return _stricmp(a, b); }

static const char *strcasestr(const char *a, const char *b) {
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

int enumFromString(const char *str, CRange<const char *> strings, bool throw_on_invalid) {
	DASSERT(str);
	for(int n = 0; n < strings.size(); n++)
		if(strcmp(str, strings[n]) == 0)
			return n;

	if(throw_on_invalid) {
		TextFormatter all_strings;
		for(int i = 0; i < strings.size(); i++)
			all_strings("%s%s", strings[i], i + 1 < strings.size() ? " " : "");
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
	int arg_id = 0;
	for(const char *c = format; *c; c++) {
		if(*c == '%') {
			out("%s", arg_id >= args.size() ? "" : args[arg_id].c_str());
			arg_id++;
		} else {
			out("%c", *c);
		}
	}

	return out.text();
}

int StringRef::compare(const StringRef &rhs) const { return strcmp(m_data, rhs.m_data); }
int StringRef::caseCompare(const StringRef &rhs) const { return strcasecmp(m_data, rhs.m_data); }
}
