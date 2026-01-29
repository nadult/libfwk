// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/platform.h"

#ifdef FWK_PLATFORM_WINDOWS
#include "sys/windows.h"
#include <imagehlp.h>
#include <signal.h>
#endif

#include "fwk/sys_base.h"

#include "fwk/enum.h"
#include "fwk/format.h"
#include "fwk/sys/backtrace.h"
#include <bit>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef FWK_PLATFORM_LINUX
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fwk {
static_assert(sizeof(i32) == 4 && sizeof(u32) == 4);
static_assert(sizeof(i64) == 8 && sizeof(u64) == 8);
static_assert(std::endian::native == std::endian::little,
			  "libfwk was only tested on little-endian platforms");

namespace {
#if defined(FWK_PLATFORM_WINDOWS)
	void (*s_user_ctrlc_func)() = 0;
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
		printf("Signal received: %s (SegCs:0x%04X Addr:0x%p)\n",
			   exceptionName(info->ExceptionRecord->ExceptionCode), info->ContextRecord->SegCs,
			   info->ExceptionRecord->ExceptionAddress);
		printf("Backtrace:\n%s\n", Backtrace::get(2, info->ContextRecord, true).format().c_str());
		return EXCEPTION_EXECUTE_HANDLER;
	}

	struct AutoSigHandler {
		AutoSigHandler() { SetUnhandledExceptionFilter(windows_exception_handler); }
	} s_auto_sig_handler;
#elif defined(FWK_PLATFORM_LINUX)
	void (*s_user_ctrlc_func)() = 0;
	void handleControlC(int) {
		if(s_user_ctrlc_func)
			s_user_ctrlc_func();
	}

	struct SignalInfo {
		int num;
		const char *message;
	} static const s_signal_infos[] = {{SIGSEGV, "Segmentation fault"},
									   {SIGILL, "Illegal instruction"},
									   {SIGFPE, "Invalid floating-point operation"},
									   {SIGBUS, "Bus error"}};

	void segfaultHandler(int, siginfo_t *sig_info, void *) {
		const char *message = nullptr;
		for(const auto &info : s_signal_infos)
			if(info.num == sig_info->si_signo)
				message = info.message;

		if(message)
			printf("%s!\n", message);
		else
			printf("Signal caught: %d!\n", (int)sig_info->si_signo);
		printf("Backtrace:\n%s\n", Backtrace::get(2, nullptr, true).format().c_str());
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
					FWK_FATAL("Error while attaching signal handler: %d", info.num);
		}
	} s_auto_sig_handler;
#endif
}

void handleCtrlC(void (*handler)()) {
#ifdef FWK_PLATFORM_WINDOWS
	s_user_ctrlc_func = handler;
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)handleControlC, TRUE);
#elif defined(FWK_PLATFORM_LINUX)
	s_user_ctrlc_func = handler;
	struct sigaction sig_int_handler;

	sig_int_handler.sa_handler = handleControlC;
	sigemptyset(&sig_int_handler.sa_mask);
	sig_int_handler.sa_flags = 0;
	sigaction(SIGINT, &sig_int_handler, NULL);
#else
	// Not supported (silently ignored)
#endif
}

#ifdef FWK_PLATFORM_WINDOWS
FWK_NO_INLINE int winGetBacktrace(Span<void *> addrs, void *context_) {
	if(!context_) {
		return CaptureStackBackTrace(0, addrs.size(), addrs.data(), 0);
	} else {
		CONTEXT *context = static_cast<CONTEXT *>(context_);
		SymInitialize(GetCurrentProcess(), 0, true);

		STACKFRAME frame;
		memset(&frame, 0, sizeof(frame));

#if defined(AMD64) || defined(__x86_64__)
		frame.AddrPC.Offset = context->Rip;
		frame.AddrStack.Offset = context->Rsp;
		frame.AddrFrame.Offset = context->Rsp;
#else
		frame.AddrPC.Offset = context->Eip;
		frame.AddrStack.Offset = context->Esp;
		frame.AddrFrame.Offset = context->Ebp;
#endif
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrStack.Mode = AddrModeFlat;
		frame.AddrFrame.Mode = AddrModeFlat;

		int count = 0;
#if defined(AMD64) || defined(__x86_64__)
		while(StackWalk64(IMAGE_FILE_MACHINE_AMD64,
#else
		while(StackWalk(IMAGE_FILE_MACHINE_I386,
#endif
						  GetCurrentProcess(), GetCurrentThread(), &frame, context, 0,
						  SymFunctionTableAccess, SymGetModuleBase, 0)) {
			addrs[count++] = (void *)frame.AddrPC.Offset;
			if(count == addrs.size())
				break;
		}

		SymCleanup(GetCurrentProcess());
		return count;
	}
}
#endif

void sleep(double sec) {
	if(sec <= 0)
		return;
#ifdef FWK_PLATFORM_WINDOWS
	// TODO: check accuracy
	SleepEx(int(sec * 1e3), true);
#else
	usleep(int(sec * 1e6));
#endif
}

double getTime() {
#ifdef FWK_PLATFORM_WINDOWS
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

Maybe<string> getEnv(ZStr var_name) {
#ifdef FWK_PLATFORM_WINDOWS
	size_t length = 0;
	if(getenv_s(&length, nullptr, 0, var_name.c_str()) != 0)
		return none;

	string result(length, '\0');
	if(getenv_s(&length, result.data(), length, var_name.c_str()) != 0)
		return none;
	result.resize(length);
	return result;
#else
	const char *value = std::getenv(var_name.c_str());
	return value ? Maybe<string>(value) : none;
#endif
}

string strError(int error_num) {
#ifdef FWK_PLATFORM_WINDOWS
	char buffer[1024];
	strerror_s(buffer, sizeof(buffer), error_num);
	return buffer;
#else
	return string(strerror(error_num));
#endif
}

void logError(const string &error) { fprintf(stderr, "%s", error.c_str()); }
}
