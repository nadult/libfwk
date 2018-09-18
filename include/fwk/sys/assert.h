// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"

namespace fwk {
namespace detail {
	template <class T1, class T2>
	[[noreturn]] void assertFailedBinary(const char *, int, const char *, const char *,
										 const char *, const T1 &, const T2 &) NOINLINE;

	template <class T1, class T2>
	[[noreturn]] void assertFailedBinary(const char *file, int line, const char *op,
										 const char *str1, const char *str2, const T1 &v1,
										 const T2 &v2) {
		assertFailed(file, line, format("%:% % %:%", str1, v1, op, str2, v2).c_str());
	}

	template <class T>
	[[noreturn]] void assertFailedHint(const char *file, int line, const char *str,
									   const char *hint, const T &hint_val) NOINLINE;
	template <class T>
	[[noreturn]] void assertFailedHint(const char *file, int line, const char *str,
									   const char *hint, const T &hint_val) {
		assertFailed(file, line, format("% | %:%", str, hint, hint_val).c_str());
	}
}

#define ASSERT_BINARY(expr1, expr2, op)                                                            \
	(((expr1)op(expr2) || (fwk::detail::assertFailedBinary(                                        \
							   __FILE__, __LINE__, FWK_STRINGIZE(op), FWK_STRINGIZE(expr1),        \
							   FWK_STRINGIZE(expr2), (expr1), (expr2)),                            \
						   0)))

#define ASSERT_EQ(expr1, expr2) ASSERT_BINARY(expr1, expr2, ==)
#define ASSERT_NE(expr1, expr2) ASSERT_BINARY(expr1, expr2, !=)
#define ASSERT_GT(expr1, expr2) ASSERT_BINARY(expr1, expr2, >)
#define ASSERT_LT(expr1, expr2) ASSERT_BINARY(expr1, expr2, <)
#define ASSERT_LE(expr1, expr2) ASSERT_BINARY(expr1, expr2, <=)
#define ASSERT_GE(expr1, expr2) ASSERT_BINARY(expr1, expr2, >=)

#define ASSERT_HINT(expr, hint)                                                                    \
	((expr) || (fwk::detail::assertFailedHint(__FILE__, __LINE__, FWK_STRINGIZE(expr),             \
											  FWK_STRINGIZE(hint), (hint)),                        \
				0))

#ifdef NDEBUG
#define DASSERT_EQ(expr1, expr2) ((void)0)
#define DASSERT_NE(expr1, expr2) ((void)0)
#define DASSERT_GT(expr1, expr2) ((void)0)
#define DASSERT_LT(expr1, expr2) ((void)0)
#define DASSERT_LE(expr1, expr2) ((void)0)
#define DASSERT_GE(expr1, expr2) ((void)0)

#define DASSERT_HINT(expr, hint) ((void)0)
#else
#define DASSERT_EQ(expr1, expr2) ASSERT_EQ(expr1, expr2)
#define DASSERT_NE(expr1, expr2) ASSERT_NE(expr1, expr2)
#define DASSERT_GT(expr1, expr2) ASSERT_GT(expr1, expr2)
#define DASSERT_LT(expr1, expr2) ASSERT_LT(expr1, expr2)
#define DASSERT_LE(expr1, expr2) ASSERT_LE(expr1, expr2)
#define DASSERT_GE(expr1, expr2) ASSERT_GE(expr1, expr2)

#define DASSERT_HINT(expr, hint) ASSERT_HINT(expr, hint)
#endif

#if defined(FWK_PARANOID)

#define PASSERT_EQ(expr1, expr2) ASSERT_EQ(expr1, expr2)
#define PASSERT_NE(expr1, expr2) ASSERT_NE(expr1, expr2)
#define PASSERT_GT(expr1, expr2) ASSERT_GT(expr1, expr2)
#define PASSERT_LT(expr1, expr2) ASSERT_LT(expr1, expr2)
#define PASSERT_LE(expr1, expr2) ASSERT_LE(expr1, expr2)
#define PASSERT_GE(expr1, expr2) ASSERT_GE(expr1, expr2)
#define PASSERT_HINT(expr, hint) ASSERT_HINT(expr, hint)

#else

#define PASSERT_EQ(expr1, expr2) ((void)0)
#define PASSERT_NE(expr1, expr2) ((void)0)
#define PASSERT_GT(expr1, expr2) ((void)0)
#define PASSERT_LT(expr1, expr2) ((void)0)
#define PASSERT_LE(expr1, expr2) ((void)0)
#define PASSERT_GE(expr1, expr2) ((void)0)
#define PASSERT_HINT(expr, hint) ((void)0)

#endif
}
