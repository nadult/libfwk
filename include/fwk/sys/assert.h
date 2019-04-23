// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"

namespace fwk {
namespace detail {
	struct AssertInfo {
		const char *file;
		const char *message;
		const char *arg_names;
		const TFFunc *funcs;
		int line;
		int arg_count;
	};
	[[noreturn]] void assertFailed(const AssertInfo *, ...);
	Error checkFailed(const AssertInfo *, ...);

	template <int V> struct Value { static constexpr int value = V; };
	template <class... T> constexpr auto countArgs(const T &...) -> Value<sizeof...(T)>;

	template <class... T> constexpr auto getArgTypes(const T &...) -> Types<T...>;

	template <class... T, EnableIfFormattible<T...>...>
	void assertFailed_(const AssertInfo &info, const T &... args) {
		assertFailed(&info, detail::getTFValue(args)...);
	}

	template <class... T, EnableIfFormattible<T...>...>
	Error checkFailed_(const AssertInfo &info, const T &... args) {
		return checkFailed(&info, detail::getTFValue(args)...);
	}

	template <class T> struct TFFuncs {};
	template <class... T> struct TFFuncs<Types<T...>> {
		static constexpr TFFunc funcs[] = {getTFFunc<T>()...};
	};
}

#define ASSERT_EX(expr, ...)                                                                       \
	{                                                                                              \
		if(__builtin_expect(!(expr), false)) {                                                     \
			using Funcs = fwk::detail::TFFuncs<decltype(fwk::detail::getArgTypes(__VA_ARGS__))>;   \
			static constexpr fwk::detail::AssertInfo info{                                         \
				__FILE__,                                                                          \
				FWK_STRINGIZE(expr),                                                               \
				FWK_STRINGIZE(__VA_ARGS__),                                                        \
				Funcs::funcs,                                                                      \
				__LINE__,                                                                          \
				decltype(fwk::detail::countArgs(__VA_ARGS__))::value};                             \
			fwk::detail::assertFailed_(info __VA_OPT__(, ) __VA_ARGS__);                           \
		}                                                                                          \
	}

#define ASSERT_FAILED(fmt, ...)                                                                    \
	{                                                                                              \
		static constexpr fwk::detail::AssertInfo info{                                             \
			__FILE__, fmt,		nullptr,                                                           \
			nullptr,  __LINE__, decltype(fwk::detail::countArgs(__VA_ARGS__))::value};             \
		fwk::detail::assertFailed_(info __VA_OPT__(, ) __VA_ARGS__);                               \
	}

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
