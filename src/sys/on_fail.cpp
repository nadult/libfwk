// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/backtrace.h"
#include "fwk/sys/error.h"
#include "fwk/sys/on_fail.h"
#include <cstdarg>

namespace fwk {

namespace detail {
	enum { max_on_assert = 64 };
	__thread OnFailInfo t_on_fail_stack[max_on_assert];
	__thread int t_on_fail_count = 0;
}

using namespace detail;

void onFailPush(OnFailInfo info) {
	PASSERT(t_on_fail_count < max_on_assert);
	t_on_fail_stack[t_on_fail_count++] = info;
}

void onFailPop() {
	PASSERT(t_on_fail_count > 0);
	t_on_fail_count--;
}

int onFailStackSize() { return t_on_fail_count; }

static __thread bool s_fail_protect = false;

vector<ErrorChunk> onFailChunks() {
	vector<ErrorChunk> chunks;
	int count = t_on_fail_count;
	chunks.reserve(count + 1);
	for(int n = 0; n < count; n++) {
		const auto &info = t_on_fail_stack[n];
		chunks.emplace_back(info.func(info.args));
	}
	return chunks;
}

Error onFailMakeError(const char *file, int line, ZStr main_message) {
	if(s_fail_protect) {
		printf("%s:%d: %s\nFATAL ERROR in libfwk (error within an error)\n", file, line,
			   main_message.c_str());
		asm("int $3");
		exit(1);
	}

	s_fail_protect = true;
	auto bt = Backtrace::get(3, nullptr, Backtrace::t_default_mode);
	auto chunks = onFailChunks();
	chunks.emplace_back(ErrorLoc{file, line}, main_message);
	Error out(move(chunks), bt);

	s_fail_protect = false;
	return out;
}

namespace detail {
	Error expectMakeError(const char *expr, const char *file, int line) {
		auto chunks = onFailChunks();
		chunks.emplace_back(ErrorLoc{file, line}, format("Failed: %", expr));
		return move(chunks);
	}
}
}
