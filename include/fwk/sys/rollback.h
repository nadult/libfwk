// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/sys_base.h"
#include "fwk_range.h"
#include <csetjmp>

namespace fwk {

using AtRollback = void (*)(void *);

// TODO: desc
// TODO: make a list of functions / objects usable while rollbacking ?
class RollbackContext {
  public:
	template <class T> using ResultOf = decltype(declval<T>()());

	// TODO: jak się zabezpieczyć przed błędami związanymi ze złymi dowiązaniami?

	// TODO: MAKE SURE ROLLBACK DOESN'T ALLOCATE EVERY TIME

	[[noreturn]] static void rollback(const Error &);
	static bool canRollback();

	// You can register/unregister a function to be called during rollback
	// There are no guarantees about the order in which they will be called
	// Nothing will be regustered if no RollbackContext is active
	static int atRollback(AtRollback func, void *arg);
	static void removeAtRollback(int index);
	static void removeAtRollback(AtRollback, void *);

	// Allocations won't be registered for garbage collection when paused
	// Typically each pause should be matched with resume on the same level
	// User have to be careful in case of multiple rollbacks
	static void pause();
	static void resume();

	struct RollbackStatus {
		int on_assert_top;
		BacktraceMode backtrace_mode;
	};
	static RollbackStatus status();

	// Only checks on current level!
	// TODO: think about adding more checks in other places ?
	static bool willRollback(CSpan<const void *>);

	// Enables RollbackContext for given function; During execution of func:
	// - each memory allocation will be registered in RollbackContext and freed
	//   automatically when rollback() is called
	// - rollback() is implemented with setjmp/longjump so no destructors will be called
	// - it's illegal to move links to rollbackable objects out of the execution scope
	// - it's illegal to create shared pointers to objects out of the execution scope
	// - it's OK to use standard containers like vector, string, HashMap, etc.
	// - it's OK to construct FileStreams in rollback mode
	// - you have to very careful with shared_ptr, static members, File handles, etc.
	// - IDEALLY func performs pure computation which doesn't modify anything outside of itself
	// TODO: label NoShared?
	// TODO: add support within shared_ptr, etc. classes...
	template <class T>
	static Expected<ResultOf<T>> begin(const T &func, Maybe<BacktraceMode> bm = none) {
		auto *context = addContext(bm);
		if(setjmp(context->jmpBuf())) {
			auto error = move(context->passed_error);
			context->dropContext();
			Expected<ResultOf<T>> out(error);
			return out;
		}

		if constexpr(std::is_void<ResultOf<T>>::value) {
			func();
			context->dropContext();
			return {};
		} else {
			auto value = func();
			context->dropContext();
			return value;
		}
	}

	template <class T> static bool tryAndHandle(const T &func, Maybe<BacktraceMode> bm = none) {
		auto result = begin(func, bm);
		if(!result) {
			result.error().print();
			return false;
		}
		return true;
	}

  private:
	struct Level;

	RollbackContext() = default;

	vector<Level> levels;
	bool is_disabled = false;
	bool is_rolling_back = false;
	bool allocation_warning = false;
	Error passed_error;

	static RollbackContext *current();
	static RollbackContext *addContext(Maybe<BacktraceMode>);

	std::jmp_buf &jmpBuf();
	void addLevel(Maybe<BacktraceMode>);
	void dropContext();
	void dropLevel();

	static void *rbAlloc(size_t size);
	static void *rbAlloc(size_t size, size_t alignment);
	static void rbFree(void *ptr);
};
}
