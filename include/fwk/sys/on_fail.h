// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/light_tuple.h"
#include "fwk/sys/error.h"

namespace fwk {
namespace detail {
	template <class T> struct StoreType { using Type = const T &; };
	template <class T> struct StoreType<T *> { using Type = const T *; };
	template <int N> struct StoreType<char[N]> { using Type = const char *; };
	template <int N> struct StoreType<const char[N]> { using Type = const char *; };

	template <class... Args>
	auto makeRefs(const Args &...) -> LightTuple<typename StoreType<Args>::Type...>;

	template <class... Args, class Func, unsigned... IS, class Refs = LightTuple<Args...>>
	ErrorChunk onFailWrapper(const char *file, int line, const Func &func,
							 const LightTuple<Args...> &refs, Seq<IS...>) {
		return {{file, line}, func(detail::GetField<IS>::get(refs)...)};
	}

	template <class... Args, unsigned... IS, class Refs = LightTuple<Args...>>
	ErrorChunk onFailWrapperSimple(const char *file, int line, const char *fmt,
								   const LightTuple<Args...> &refs, Seq<IS...>) {
		return {{file, line}, format(fmt, detail::GetField<IS>::get(refs)...)};
	}
}

struct OnFailInfo {
	using FuncType = ErrorChunk (*)(const void *);
	FuncType func;
	const void *args;
};

void onFailPush(OnFailInfo info);
void onFailPop();
int onFailStackSize();

struct OnFailGuard {
	~OnFailGuard() { onFailPop(); }
};

Error onFailMakeError(const char *file, int line, ZStr main_message);

// Use it just like you would use fwk::format
// Maximum nr of additional arguments: 8
// Maximum supported depth: 64
//
// Example use:
//   ON_FAIL_FUNC(([](const MyClass &ref) { return ref.name(); }), *this);
//
//   void func(const char*, string&);
//   ON_FAIL_FUNC(func, "some string", &string_in_local_context);
//
//   ON_FAIL("Some text: % and: %", argument1, int3(20, 30, 40));
//
#define ON_FAIL_FUNC(func_impl, ...)                                                               \
	const decltype(fwk::detail::makeRefs(__VA_ARGS__)) FWK_JOIN(_refs_, __LINE__){__VA_ARGS__};    \
	{                                                                                              \
		using namespace fwk::detail;                                                               \
		using RefType = decltype(FWK_JOIN(_refs_, __LINE__));                                      \
		auto func = [](const void *prefs) -> ErrorChunk {                                          \
			auto func_ref = (func_impl);                                                           \
			return onFailWrapper(__FILE__, __LINE__, func_ref, *(const RefType *)prefs,            \
								 GenSeq<RefType::count>());                                        \
		};                                                                                         \
		fwk::onFailPush({func, (const void *)(&FWK_JOIN(_refs_, __LINE__))});                      \
	}                                                                                              \
	fwk::OnFailGuard FWK_JOIN(_guard_, __LINE__);

#define ON_FAIL(format_str, ...)                                                                   \
	const decltype(fwk::detail::makeRefs(__VA_ARGS__)) FWK_JOIN(_refs_, __LINE__){__VA_ARGS__};    \
	{                                                                                              \
		using namespace fwk::detail;                                                               \
		using RefType = decltype(FWK_JOIN(_refs_, __LINE__));                                      \
		auto func = [](const void *prefs) -> ErrorChunk {                                          \
			return onFailWrapperSimple(__FILE__, __LINE__, format_str, *(const RefType *)prefs,    \
									   GenSeq<RefType::count>());                                  \
		};                                                                                         \
		fwk::onFailPush({func, (const void *)(&FWK_JOIN(_refs_, __LINE__))});                      \
	}                                                                                              \
	fwk::OnFailGuard FWK_JOIN(_guard_, __LINE__);

#ifdef NDEBUG
#define ON_DFAIL(func, ptr)
#define ON_DFAIL_FUNC(func, ...)
#else
#define ON_DFAIL_FUNC(func, ...) ON_FAIL(func, __VA_ARGS__)
#define ON_DFAIL(format, ...) ON_FAIL(format, __VA_ARGS__)
#endif
}
