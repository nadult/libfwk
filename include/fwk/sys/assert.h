// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/assert_info.h"

namespace fwk {

[[noreturn]] void assertFailed_(const AssertInfo *, ...);

#define ASSERT_EX(expr, ...)                                                                       \
	(__builtin_expect((expr), true) ||                                                             \
	 (ASSERT_WITH_PARAMS(fwk::assertFailed_, FWK_STRINGIZE(expr) __VA_OPT__(, ) __VA_ARGS__), 0))

#define ASSERT_FAILED(...) ASSERT_FORMATTED(fwk::assertFailed, __VA_ARGS__)

#ifdef NDEBUG
#define DASSERT_EX(...) ((void)0)
#else
#define DASSERT_EX(...) ASSERT_EX(__VA_ARGS__)
#endif

#if defined(FWK_PARANOID)
#define PASSERT_EX(...) ASSERT_EX(__VA_ARGS__)
#else
#define PASSERT_EX(...) ((void)0)
#endif

#define ASSERT_BINARY(e1, e2, op) ASSERT_EX((e1)op(e2), e1, e2)

#define ASSERT_EQ(expr1, expr2) ASSERT_BINARY(expr1, expr2, ==)
#define ASSERT_NE(expr1, expr2) ASSERT_BINARY(expr1, expr2, !=)
#define ASSERT_GT(expr1, expr2) ASSERT_BINARY(expr1, expr2, >)
#define ASSERT_LT(expr1, expr2) ASSERT_BINARY(expr1, expr2, <)
#define ASSERT_LE(expr1, expr2) ASSERT_BINARY(expr1, expr2, <=)
#define ASSERT_GE(expr1, expr2) ASSERT_BINARY(expr1, expr2, >=)

#ifdef NDEBUG
#define DASSERT_EQ(expr1, expr2) ((void)0)
#define DASSERT_NE(expr1, expr2) ((void)0)
#define DASSERT_GT(expr1, expr2) ((void)0)
#define DASSERT_LT(expr1, expr2) ((void)0)
#define DASSERT_LE(expr1, expr2) ((void)0)
#define DASSERT_GE(expr1, expr2) ((void)0)

#else
#define DASSERT_EQ(expr1, expr2) ASSERT_EQ(expr1, expr2)
#define DASSERT_NE(expr1, expr2) ASSERT_NE(expr1, expr2)
#define DASSERT_GT(expr1, expr2) ASSERT_GT(expr1, expr2)
#define DASSERT_LT(expr1, expr2) ASSERT_LT(expr1, expr2)
#define DASSERT_LE(expr1, expr2) ASSERT_LE(expr1, expr2)
#define DASSERT_GE(expr1, expr2) ASSERT_GE(expr1, expr2)

#endif

#if defined(FWK_PARANOID)

#define PASSERT_EQ(expr1, expr2) ASSERT_EQ(expr1, expr2)
#define PASSERT_NE(expr1, expr2) ASSERT_NE(expr1, expr2)
#define PASSERT_GT(expr1, expr2) ASSERT_GT(expr1, expr2)
#define PASSERT_LT(expr1, expr2) ASSERT_LT(expr1, expr2)
#define PASSERT_LE(expr1, expr2) ASSERT_LE(expr1, expr2)
#define PASSERT_GE(expr1, expr2) ASSERT_GE(expr1, expr2)

#else

#define PASSERT_EQ(expr1, expr2) ((void)0)
#define PASSERT_NE(expr1, expr2) ((void)0)
#define PASSERT_GT(expr1, expr2) ((void)0)
#define PASSERT_LT(expr1, expr2) ((void)0)
#define PASSERT_LE(expr1, expr2) ((void)0)
#define PASSERT_GE(expr1, expr2) ((void)0)

#endif
}
