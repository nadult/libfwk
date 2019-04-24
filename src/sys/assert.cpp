// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/assert.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/error.h"
#include "fwk/sys/on_fail.h"
#include <cstdarg>
#include <stdarg.h>

namespace fwk {

void assertFailed_(const AssertInfo *info, ...) {
	TextFormatter out;
	auto fmt = info->preFormat(out, "Assert failed: ");

	va_list ap;
	va_start(ap, info);
	out.append_(fmt.c_str(), info->arg_count, info->funcs, ap);
	va_end(ap);

	onFailMakeError(info->file, info->line, out.text()).print();

	asm("int $3");
	exit(1);
}

Error checkFailed(const AssertInfo *info, ...) {
	TextFormatter out;
	auto fmt = info->preFormat(out, "Check failed: ");

	va_list ap;
	va_start(ap, info);
	out.append_(fmt.c_str(), info->arg_count, info->funcs, ap);
	va_end(ap);

	return onFailMakeError(info->file, info->line, out.text());
}

void failedExpected(const char *file, int line, const Error &error) {
	fatalError(file, line, "Getting value from invalid Expected<>: %s", toString(error).c_str());
}

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
}
