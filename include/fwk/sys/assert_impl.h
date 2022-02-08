// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"

namespace fwk {

#define PP_THIRD_ARG(a, b, c, ...) c
#define VA_OPT_SUPPORTED_I(...) PP_THIRD_ARG(__VA_OPT__(, ), true, false, )
#define VA_OPT_SUPPORTED VA_OPT_SUPPORTED_I(?)

#if !(VA_OPT_SUPPORTED) && !(_MSC_VER >= 1925)
#error "__VA_OPT__ support is required for assertions to work"
#endif

#undef PP_THIRD_ARG
#undef VA_OPT_SUPPORTED_I
#undef VA_OPT_SUPPORTED

namespace detail {
	struct AssertInfo {
		const char *file;
		const char *message;
		const char *arg_names;
		const detail::TFFunc *funcs;
		int line;
		int arg_count;

		string preFormat(TextFormatter &, const char *prefix) const;
	};

	using CallFunc = void (*)(const AssertInfo *, ...);
	template <class Strings, int line, class Func, class... T>
	inline auto assertCall(const Func &func, const T &...args) {
		static_assert((is_formattible<T> && ...));
		using Funcs = decltype(detail::formatFuncs(args...));
		constexpr Strings strings;
		static constexpr AssertInfo info{strings.file, strings.msg, strings.arg_names,
										 Funcs::funcs, line,		Funcs::count};
		return func(&info, detail::getTFValue(args)...);
	}

	[[noreturn]] void assertFailed(const AssertInfo *, ...);
	void raiseException(const AssertInfo *, ...) EXCEPT;
	void checkFailed(const AssertInfo *, ...) EXCEPT;
	Error makeError(const AssertInfo *, ...);

// Message format: text + list of named params
#define _ASSERT_WITH_PARAMS(func, message, ...)                                                    \
	({                                                                                             \
		struct Strings {                                                                           \
			const char *file = __FILE__, *msg = message, *arg_names = FWK_STRINGIZE(__VA_ARGS__);  \
		};                                                                                         \
		fwk::detail::assertCall<Strings, __LINE__>(fwk::detail::func __VA_OPT__(, ) __VA_ARGS__);  \
	})

// Message format: format text + its arguments
#define _ASSERT_FORMATTED(func, fmt, ...)                                                          \
	({                                                                                             \
		struct Strings {                                                                           \
			const char *file = __FILE__, *msg = fmt, *arg_names = "";                              \
		};                                                                                         \
		fwk::detail::assertCall<Strings, __LINE__>(fwk::detail::func __VA_OPT__(, ) __VA_ARGS__);  \
	})

}

}
