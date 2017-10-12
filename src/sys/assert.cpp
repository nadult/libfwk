// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/assert.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/error.h"
#include "fwk/sys/rollback.h"
#include <cstdarg>

namespace fwk {

namespace detail {
	__thread OnAssertInfo t_on_assert_stack[64];
	__thread uint t_on_assert_count = 0;
}

static __thread bool s_assert_protect = false;

static Error makeError(const char *file, int line, const char *main_message) {
	if(s_assert_protect) {
		printf("%s:%d: %s\nFATAL ERROR in libfwk (error within an error)\n", file, line,
			   main_message);
		asm("int $3");
		exit(1);
	}

	s_assert_protect = true;

	auto roll_status = RollbackContext::status();
	auto bt = Backtrace::get(2, nullptr, roll_status.backtrace_mode);

	vector<ErrorChunk> chunks;

	int start = 0; //roll_status.on_assert_top;
	int count = detail::t_on_assert_count - start;

	chunks.reserve(count + 1);
	for(int n : intRange(count)) {
		const auto &info = detail::t_on_assert_stack[start + n];
		chunks.emplace_back(info.func(info.args));
	}
	chunks.emplace_back(main_message, file, line);
	Error out(move(chunks), bt);

	s_assert_protect = false;
	return out;
}

void fatalError(const char *file, int line, const char *fmt, ...) {
	char buffer[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	makeError(file, line, buffer).print();
	asm("int $3");
#endif
	exit(1);
}

void assertFailed(const char *file, int line, const char *text) {
	char buffer[4096];
	snprintf(buffer, sizeof(buffer), "%s:%d: Assertion failed: %s", file, line, text);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	makeError(file, line, buffer).print();
	asm("int $3");
#endif
	exit(1);
}

void checkFailed(const char *file, int line, const char *fmt, ...) {
	char buffer[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	RollbackContext::pause();
	auto error = makeError(file, line, buffer);
	RollbackContext::resume();

	if(RollbackContext::canRollback())
		RollbackContext::rollback(move(error));

	error.print();
	asm("int $3");
	exit(1);
#endif
}
}
