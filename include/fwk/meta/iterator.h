// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta/operator.h"
#include "fwk/sys_base.h"
#include <climits>

namespace fwk {

namespace detail {

	template <class T> struct IterInfo {
		template <class U> static constexpr auto test(U *) -> decltype(*DECLVAL(U));
		template <class U> static constexpr void test(...);
		using BaseType = decltype(test<T>(DECLVAL(T *)));
	};

	template <class T> struct IsFwdIter {
		template <class U>
		static constexpr auto test(U *) -> decltype(++DECLVAL(U), DECLVAL(U) != DECLVAL(U));
		template <class U> static constexpr void test(...);

		static constexpr bool value = is_same<decltype(test<T>(DECLVAL(T *))), bool> &&
									  !is_same<typename IterInfo<T>::BaseType, void>;
	};
	template <class T> struct IsFwdIter<T *> { static constexpr bool value = true; };

}

template <class IT> static constexpr bool is_forward_iter = detail::IsFwdIter<IT>::value;
template <class IT>
static constexpr bool is_random_iter =
	is_forward_iter<IT> &&std::is_integral<SubResult<IT, IT>>::value;

template <class IT> using IterBase = typename detail::IterInfo<IT>::BaseType;

template <class IT, EnableIf<is_forward_iter<IT>>...> auto distance(IT begin, IT end) {
	long long ret;
	if constexpr(is_random_iter<IT>) {
		PASSERT(end >= begin);
		ret = end - begin;
	} else {
		ret = 0;
		while(begin != end)
			++begin, ++ret;
	}

	PASSERT(ret < INT_MAX);
	return int(ret);
}

}
