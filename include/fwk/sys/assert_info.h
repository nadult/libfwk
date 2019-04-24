// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"

namespace fwk {

struct AssertInfo {
	const char *file;
	const char *message;
	const char *arg_names;
	const detail::TFFunc *funcs;
	int line;
	int arg_count;

	string preFormat(TextFormatter &, const char *prefix) const;
};

namespace detail {
	using CallFunc = void (*)(const AssertInfo *, ...);
	template <class SFile, int line, class SMessage, class SArgNames, class Func, class... T>
	inline auto assertCall(const Func &func, const T &... args) {
		static_assert((is_formattible<T> && ...));
		using Funcs = decltype(detail::formatFuncs(args...));
		static constexpr AssertInfo info{SFile::text,  SMessage::text, SArgNames::text,
										 Funcs::funcs, line,		   Funcs::count};
		return func(&info, detail::getTFValue(args)...);
	}
}

// Message format: text + list of named params
#define ASSERT_WITH_PARAMS(func, message, ...)                                                     \
	fwk::detail::assertCall<decltype(__FILE__ ""_ss), __LINE__, decltype(message ""_ss),           \
							decltype(FWK_STRINGIZE(__VA_ARGS__) ""_ss)>(func __VA_OPT__(, )        \
																			__VA_ARGS__)

// Message format: format text + its arguments
#define ASSERT_FORMATTED(func, fmt, ...)                                                           \
	fwk::detail::assertCall<decltype(__FILE__ ""_ss), __LINE__, decltype(fmt ""_ss),               \
							decltype(""_ss)>(func __VA_OPT__(, ) __VA_ARGS__)

}
