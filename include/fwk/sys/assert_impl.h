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
		array<const char *, 8> arg_names;
		const detail::TFFunc *funcs;
		int line;
		int arg_count;

		string preFormat(TextFormatter &, const char *prefix) const;
	};

	using CallFunc = void (*)(const AssertInfo *, ...);
	template <ConstString file, int line, ConstString message, ConstString... arg_names, class Func,
			  c_formattible... T>
	inline auto assertCall(const Func &func, const T &...args) {
		static_assert(sizeof...(arg_names) <= 8);
		using Funcs = decltype(detail::formatFuncs(args...));
		static constexpr AssertInfo info{file.string,  message.string, {arg_names.string...},
										 Funcs::funcs, line,		   Funcs::count};
		return func(&info, detail::getTFValue(args)...);
	}

	[[noreturn]] void assertFailed(const AssertInfo *, ...);
	void raiseException(const AssertInfo *, ...) EXCEPT;
	void checkFailed(const AssertInfo *, ...) EXCEPT;
	Error makeError(const AssertInfo *, ...);

// Message format: text + list of named params
#define _ASSERT_WITH_PARAMS(func, message, ...)                                                    \
	({                                                                                             \
		fwk::detail::assertCall<__FILE__, __LINE__, message, FWK_STRINGIZE_MANY(__VA_ARGS__)>(     \
			fwk::detail::func __VA_OPT__(, ) __VA_ARGS__);                                         \
	})

// Message format: format text + its arguments
#define _ASSERT_FORMATTED(func, fmt, ...)                                                          \
	({                                                                                             \
		fwk::detail::assertCall<__FILE__, __LINE__, fmt>(fwk::detail::func __VA_OPT__(, )          \
															 __VA_ARGS__);                         \
	})

}

}
