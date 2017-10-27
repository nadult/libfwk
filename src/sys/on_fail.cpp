// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/backtrace.h"
#include "fwk/sys/error.h"
#include "fwk/sys/on_fail.h"
#include "fwk/sys/rollback.h"
#include <cstdarg>

namespace fwk {

namespace detail {
	enum { max_on_assert = 64 };
	__thread OnFailInfo t_on_fail_stack[max_on_assert];
	__thread int t_on_fail_count = 0;
}

void onFailPush(OnFailInfo info) {
	PASSERT(t_on_fail_count < max_on_assert);
	detail::t_on_fail_stack[detail::t_on_fail_count++] = info;
}

void onFailPop() {
	PASSERT(t_on_fail_count > 0);
	detail::t_on_fail_count--;
}

int onFailStackSize() { return detail::t_on_fail_count; }

static __thread bool s_fail_protect = false;

Error onFailMakeError(const char *file, int line, const char *main_message) {
	if(s_fail_protect) {
		printf("%s:%d: %s\nFATAL ERROR in libfwk (error within an error)\n", file, line,
			   main_message);
		asm("int $3");
		exit(1);
	}

	s_fail_protect = true;

	auto roll_status = RollbackContext::status();
	auto bt = Backtrace::get(3, nullptr, roll_status.backtrace_mode);

	vector<ErrorChunk> chunks;

	int start = 0; //roll_status.on_assert_top;
	int count = detail::t_on_fail_count - start;

	chunks.reserve(count + 1);
	for(int n = 0; n < count; n++) {
		const auto &info = detail::t_on_fail_stack[start + n];
		chunks.emplace_back(info.func(info.args));
	}
	chunks.emplace_back(main_message, file, line);
	Error out(move(chunks), bt);

	s_fail_protect = false;
	return out;
}
}
