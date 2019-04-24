// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/sys/assert_info.h"
#include "fwk/sys/error.h"
#include "fwk/sys_base.h"

namespace fwk {

// Simple exception system for libfwk:
//
// Exceptions are simply errors kept on a per-thread stack
// Raising an exception doesn't affect control flow in any way: error is simply added to the stack.
// Only functions which are marked with EXCEPT can raise an exception and leave it on the stack.
// User have to manually check if there are any exceptions present when necessary.
//
// This system is designed to interoperate with Expected<> and EXPECT_ macros.

#define CHECK_EX(expr, ...)                                                                        \
	(__builtin_expect((expr), true) ||                                                             \
	 (ASSERT_WITH_PARAMS(fwk::raiseException, FWK_STRINGIZE(expr) __VA_OPT__(, ) __VA_ARGS), 0))

#define CHECK(expr)                                                                                \
	(__builtin_expect((expr), true) ||                                                             \
	 (ASSERT_WITH_PARAMS(fwk::raiseException, FWK_STRINGIZE(expr)), 0))

#define RAISE(...) ASSERT_FORMATTED(fwk::raiseException_, __VA_ARGS__)

#define EXCEPTION(...) RAISE(__VA_ARGS__)

namespace detail {
	extern __thread int t_num_exceptions;
}

inline bool anyExceptions() { return detail::t_num_exceptions > 0; }
inline bool noExceptions() { return detail::t_num_exceptions == 0; }
inline int numExceptions() { return detail::t_num_exceptions; }

// Clears exceptions and returns them
vector<Error> getExceptions();
Error getMergedExceptions();
void clearExceptions();

// Adds new exception to the stack
void raiseException(Error, int bt_skip = 0) EXCEPT;
void raiseException_(const AssertInfo *, ...) EXCEPT;
}
