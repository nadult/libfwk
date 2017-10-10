// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_base.h"
#include "fwk_format.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdarg.h>

#ifdef FWK_TARGET_LINUX
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
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
		} catch(const Exception &ex) { printf("Failed:\n%s\n", ex.what()); }
		exit(1);
	}

	return out;
}

StringRef Tokenizer::next() {
	const char *start = m_str;
	while(*m_str && *m_str != m_delim)
		m_str++;
	const char *end = m_str++;

	return {start, (int)(end - start)};
}

#if defined(FWK_TARGET_LINUX)

namespace {

	void (*s_user_ctrlc_func)() = 0;

	void handleControlC(int) {
		if(s_user_ctrlc_func)
			s_user_ctrlc_func();
	}

	struct SignalInfo {
		int num;
		const char *message;
	}
	static const s_signal_infos[] = {
		{ SIGSEGV, "Segmentation fault" },
		{ SIGILL, "Illegal instruction" },
		{ SIGFPE, "Invalid floating-point operation"},
		{ SIGBUS, "Bus error" }};

	void segfaultHandler(int, siginfo_t *sig_info, void *) {
		const char *message = nullptr;
		for(const auto &info : s_signal_infos)
			if(info.num == sig_info->si_signo)
				message = info.message;

		if(message)
			printf("%s!\n", message);
		else
			printf("Signal caught: %d!\n", (int)sig_info->si_signo);

		printf("Backtrace:\n%s\n", Backtrace::get(2).analyze(true).c_str());
		exit(1);
	}

	struct AutoSigHandler {
		AutoSigHandler() {
			struct sigaction sa;
			sa.sa_flags = SA_SIGINFO;
			sigemptyset(&sa.sa_mask);
			sa.sa_sigaction = segfaultHandler;
			for(const auto &info : s_signal_infos)
				if(sigaction(info.num, &sa, NULL) == -1)
					FATAL("Error while attaching signal handler: %d", info.num);
		}
	} s_auto_sig_handler;
}

void handleCtrlC(void (*handler)()) {
	s_user_ctrlc_func = handler;
	struct sigaction sig_int_handler;

	sig_int_handler.sa_handler = handleControlC;
	sigemptyset(&sig_int_handler.sa_mask);
	sig_int_handler.sa_flags = 0;
	sigaction(SIGINT, &sig_int_handler, NULL);
}

#endif

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

#ifndef _WIN32
void sleep(double sec) {
	if(sec <= 0)
		return;
	usleep(int(sec * 1e6));
}

double getTime() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_nsec / 1.0e9 + ts.tv_sec;
}
#endif

Exception::Exception(string text) : m_text(move(text)), m_backtrace(Backtrace::get(2)) {}
Exception::Exception(string text, Backtrace bt) : m_text(move(text)), m_backtrace(move(bt)) {}

const char *Exception::what() const noexcept {
	static char thread_local buffer[4096];

	try {
		TextFormatter fmt;
		fmt("%\nBacktrace:\n%", text(), backtrace());
		int len = min(fmt.length(), (int)arraySize(buffer) - 1);
		memcpy(buffer, fmt.c_str(), len);
		buffer[len] = 0;
	} catch(...) { return text(); }
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
	} catch(...) {}
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

int enumFromString(const char *str, CSpan<const char *> strings, bool throw_on_invalid) {
	DASSERT(str);
	for(int n = 0; n < strings.size(); n++)
		if(strcmp(str, strings[n]) == 0)
			return n;

	if(throw_on_invalid) {
		THROW("Error when parsing enum: couldn't match \"%s\" to (%s)", str,
			  toString(strings).c_str());
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
	m_size = new_size;
}

void BitVector::clear(bool value) {
	memset(m_data.data(), value ? 0xff : 0, m_data.size() * sizeof(base_type));
}

int StringRef::compare(const StringRef &rhs) const { return strcmp(m_data, rhs.m_data); }
int StringRef::caseCompare(const StringRef &rhs) const { return strcasecmp(m_data, rhs.m_data); }
}
