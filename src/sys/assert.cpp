// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/assert.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/error.h"
#include "fwk/sys/on_fail.h"
#include <cstdarg>

namespace fwk {

void fatalError(const char *file, int line, const char *fmt, ...) {
	char buffer[4096], *bptr = buffer;
	bptr += snprintf(buffer, sizeof(buffer), "FATAL: ");

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(bptr, sizeof(buffer) - (bptr - buffer), fmt, ap);
	va_end(ap);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	onFailMakeError(file, line, buffer).print();
	asm("int $3");
#endif
	exit(1);
}

void assertFailed(const char *file, int line, const char *text) {
	char buffer[4096];
	snprintf(buffer, sizeof(buffer), "Assertion failed: %s", text);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	onFailMakeError(file, line, buffer).print();
	asm("int $3");
#endif
	exit(1);
}

void checkFailed(const char *file, int line, const char *fmt, ...) {
	char buffer[4096], *bptr = buffer;
	bptr += snprintf(buffer, sizeof(buffer), "Check failed: ");

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(bptr, sizeof(buffer) - (bptr - buffer), fmt, ap);
	va_end(ap);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	auto error = onFailMakeError(file, line, buffer);

	error.print();
	asm("int $3");
	exit(1);
#endif
}

void checkFailed(const char *file, int line, Error error) {
	string text = format("Check failed: %", error);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", text.c_str());
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", text.c_str());
	emscripten_force_exit(1);
#else
	auto error2 = onFailMakeError(file, line, text.c_str());
	error2.values.swap(error.values);

	error2.print();
	asm("int $3");
	exit(1);
#endif
}
}
