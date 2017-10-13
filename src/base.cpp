// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_base.h"

#include "fwk/enum.h"
#include "fwk/format.h"
#include "fwk/sys/backtrace.h"
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
		CHECK_FAILED("error while executing command: '%s'", cmd.c_str());
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

void logError(const string &error) { fprintf(stderr, "%s", error.c_str()); }

int enumFromString(const char *str, CSpan<const char *> strings, bool check_if_invalid) {
	DASSERT(str);
	for(int n = 0; n < strings.size(); n++)
		if(strcmp(str, strings[n]) == 0)
			return n;

	if(check_if_invalid)
		CHECK_FAILED("Error when parsing enum: couldn't match \"%s\" to (%s)", str,
					 toString(strings).c_str());
	return -1;
}
}
