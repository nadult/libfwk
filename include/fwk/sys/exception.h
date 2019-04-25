// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/assert_impl.h"
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

// When expression evaluates to false, exception is raised
// Additional arguments can be passed to make error more informative.
// Example: CHECK(str.size() > min_size, str.size(), min_size);
#define CHECK(expr, ...)                                                                           \
	(__builtin_expect(!!(expr), true) ||                                                           \
	 (_ASSERT_WITH_PARAMS(checkFailed, FWK_STRINGIZE(expr) __VA_OPT__(, ) __VA_ARGS__), 0))

// Raises an exception
// Example: RAISE("Invalid nr of elements: % (expected: %)", elems.size(), required_count);
#define RAISE(...) _ASSERT_FORMATTED(raiseException, __VA_ARGS__)

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
void printExceptions();

// Adds new exception to the stack
void raiseException(Error, int bt_skip = 0) EXCEPT;
}