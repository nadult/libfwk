// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

// This file should be included by sys_base.cpp

#if !defined(FWK_TARGET_MINGW)
#error "This file should only be compiled for MinGW target"
#else

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imagehlp.h>
#undef ERROR

#include "fwk/sys_base.h"
#include "fwk/sys/backtrace.h"

#include <ctime>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace fwk {

namespace {

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
		printf("Backtrace:\n%s\n", Backtrace::get(2, info->ContextRecord).analyze(false).c_str());
		return EXCEPTION_EXECUTE_HANDLER;
	}

	struct AutoSigHandler {
		AutoSigHandler() { SetUnhandledExceptionFilter(windows_exception_handler); }
	} s_auto_sig_handler;
}

void handleCtrlC(void (*handler)()) {
	s_user_ctrlc_func = handler;
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)handleControlC, TRUE);
}

void *winLoadFunction(const char *name) {
	PROC func = wglGetProcAddress(name);
	if(!func)
		FATAL("Error while importing OpenGL function: %s", name);
	return (void *)func;
}

void winGetBacktrace(vector<void *> &addrs, int skip, void *context_) {
	if(!context_) {
		addrs.resize(64);
		int count = CaptureStackBackTrace(max(0, (int)skip - 1), addrs.size(), &addrs[0], 0);
		addrs.resize(count);
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

#if defined(AMD64) || defined(__x86_64__)
		while(StackWalk64(IMAGE_FILE_MACHINE_AMD64,
#else
		while(StackWalk(IMAGE_FILE_MACHINE_I386,
#endif
						  GetCurrentProcess(), GetCurrentThread(), &frame, context, 0,
						  SymFunctionTableAccess, SymGetModuleBase, 0)) {
			addrs.emplace_back((void *)frame.AddrPC.Offset);
		}

		SymCleanup(GetCurrentProcess());
	}
}

void sleep(double sec) {
	if(sec <= 0)
		return;
	// TODO: check accuracy
	SleepEx(int(sec * 1e3), true);
}

double getTime() {
	__int64 tmp;
	QueryPerformanceFrequency((LARGE_INTEGER *)&tmp);
	double freq = double(tmp == 0 ? 1 : tmp);
	if(QueryPerformanceCounter((LARGE_INTEGER *)&tmp))
		return double(tmp) / freq;

	clock_t c = clock();
	return double(c) / double(CLOCKS_PER_SEC);
}

int threadId() { return GetCurrentThreadId(); }
}

#endif
