// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/assert.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/error.h"
#include "fwk/sys/on_fail.h"
#include <cstdarg>
#include <stdarg.h>

#ifdef FWK_PLATFORM_HTML
#include <emscripten.h>
#endif

namespace fwk {

void fatalError(const char *file, int line, const char *fmt, ...) {
	char buffer[4096], *bptr = buffer;
	bptr += snprintf(buffer, sizeof(buffer), "FATAL: ");

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(bptr, sizeof(buffer) - (bptr - buffer), fmt, ap);
	va_end(ap);

#ifdef FWK_PLATFORM_HTML
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	Backtrace::t_is_enabled = true;
	onFailMakeError(file, line, buffer).print();
#ifdef FWK_PLATFORM_MSVC
	__debugbreak();
#endif
#endif
	exit(1);
}

void fatalError(const Error &error) {
	error.print();
	exit(1);
}

void assertFailed(const char *file, int line, const char *text) {
	char buffer[4096];

#ifdef FWK_PLATFORM_HTML
	snprintf(buffer, sizeof(buffer), "%s:%d: Assertion failed: %s", file, line, text);
#else
	snprintf(buffer, sizeof(buffer), "Assertion failed: %s", text);
#endif

#ifdef FWK_PLATFORM_HTML
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	Backtrace::t_is_enabled = true;
	onFailMakeError(file, line, buffer).print();
#ifdef FWK_PLATFORM_MSVC
	__debugbreak();
#endif
#endif
	exit(1);
}

void failedNotInRange(int index, int begin, int end) {
	FWK_FATAL("Value %d out of range: <%d; %d)", index, begin, end);
}

namespace detail {
	void invalidIndexSparse(int idx, int end_idx) {
		FWK_FATAL("Invalid index in SparseVector: %d; end index: %d", idx, end_idx);
	}
}
}
