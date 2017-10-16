// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/light_tuple.h"
#include "fwk/sys/error.h"

namespace fwk {
struct Error;

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

	template <class T> struct StoreType { using Type = const T &; };
	template <class T> struct StoreType<T *> { using Type = const T *; };
	template <int N> struct StoreType<char[N]> { using Type = const char *; };
	template <int N> struct StoreType<const char[N]> { using Type = const char *; };

	template <class... Args>
	auto makeRefFields(const Args &...) -> LightTuple<typename StoreType<Args>::Type...>;

	template <class... Args, class Func, unsigned... IS, class Refs = LightTuple<Args...>>
	ErrorChunk onAssertWrapper(const char *file, int line, const Func &func,
							   const LightTuple<Args...> &refs, Seq<IS...>) {
		return {func(detail::GetField<IS>::get(refs)...), file, line};
	}

	template <class... Args, unsigned... IS, class Refs = LightTuple<Args...>>
	ErrorChunk onAssertWrapperSimple(const char *file, int line, const char *fmt,
									 const LightTuple<Args...> &refs, Seq<IS...>) {
		return {format(fmt, detail::GetField<IS>::get(refs)...), file, line};
	}

	struct OnAssertInfo {
		using FuncType = ErrorChunk (*)(const void *);
		FuncType func;
		const void *args;

		void print() const { fwk::print("Assert:\n%\n", func(args)); }
	};

	enum { max_on_assert = 64 };

	// TODO: hide this properly (add interface)
	extern __thread OnAssertInfo t_on_assert_stack[64];
	extern __thread uint t_on_assert_count;

	struct OnAssertGuard {
		~OnAssertGuard() { t_on_assert_count--; }
	};
}

// TODO: describe how it works
// TODO: ON_FAIL ?
// TODO: format style asserts ?
//
// Use it just like you would use fwk::format
// Maximum nr of additional arguments: 8
// Maximum supported depth: 64
//
// Example use:
//
// ON_ASSERT_FUNC(([](const MyClass &ref) { return ref.name(); }), *this);
//
// void func(const char*, string&);
// ON_ASSERT_FUNC(func, "some string", &string_in_local_context);
//
// ON_ASSERT("Some text: % and: %", argument1, int3(20, 30, 40));
//
#define ON_ASSERT_FUNC(func_impl, ...)                                                             \
	const decltype(detail::makeRefFields(__VA_ARGS__)) FWK_JOIN(_refs_, __LINE__){__VA_ARGS__};    \
	{                                                                                              \
		using namespace fwk::detail;                                                               \
		using RefType = decltype(FWK_JOIN(_refs_, __LINE__));                                      \
		auto func = [](const void *prefs) -> ErrorChunk {                                          \
			auto func_ref = (func_impl);                                                           \
			return onAssertWrapper(__FILE__, __LINE__, func_ref, *(const RefType *)prefs,          \
								   GenSeq<RefType::count>());                                      \
		};                                                                                         \
		PASSERT(t_on_assert_count < max_on_assert);                                                \
		t_on_assert_stack[t_on_assert_count++] = {func,                                            \
												  (const void *)(&FWK_JOIN(_refs_, __LINE__))};    \
	}                                                                                              \
	fwk::detail::OnAssertGuard FWK_JOIN(_guard_, __LINE__);

#define ON_ASSERT(format_str, ...)                                                                 \
	const decltype(detail::makeRefFields(__VA_ARGS__)) FWK_JOIN(_refs_, __LINE__){__VA_ARGS__};    \
	{                                                                                              \
		using namespace fwk::detail;                                                               \
		using RefType = decltype(FWK_JOIN(_refs_, __LINE__));                                      \
		auto func = [](const void *prefs) -> ErrorChunk {                                          \
			return onAssertWrapperSimple(__FILE__, __LINE__, format_str, *(const RefType *)prefs,  \
										 GenSeq<RefType::count>());                                \
		};                                                                                         \
		PASSERT(t_on_assert_count < max_on_assert);                                                \
		t_on_assert_stack[t_on_assert_count++] = {func,                                            \
												  (const void *)(&FWK_JOIN(_refs_, __LINE__))};    \
	}                                                                                              \
	fwk::detail::OnAssertGuard FWK_JOIN(_guard_, __LINE__);

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
#define ON_DASSERT(func, ptr)
#else
#define DASSERT_EQ(expr1, expr2) ASSERT_EQ(expr1, expr2)
#define DASSERT_NE(expr1, expr2) ASSERT_NE(expr1, expr2)
#define DASSERT_GT(expr1, expr2) ASSERT_GT(expr1, expr2)
#define DASSERT_LT(expr1, expr2) ASSERT_LT(expr1, expr2)
#define DASSERT_LE(expr1, expr2) ASSERT_LE(expr1, expr2)
#define DASSERT_GE(expr1, expr2) ASSERT_GE(expr1, expr2)

#define DASSERT_HINT(expr, hint) ASSERT_HINT(expr, hint)
#define ON_DASSERT_FUNC(func, ...) ON_ASSERT(func, __VA_ARGS__)
#define ON_DASSERT(format, ...) ON_ASSERT(format, __VA_ARGS__)
#endif
}
