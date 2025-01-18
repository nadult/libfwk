// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta/operator.h"
#include "fwk/sys_base.h"

namespace fwk {

template <class T>
constexpr bool is_forward_iter =
	!is_same<DereferenceResult<T>, InvalidType> && !is_same<PreIncrementResult<T>, InvalidType> &&
	equality_comparable<T, T>;

template <class T>
constexpr bool is_random_iter = is_forward_iter<T> && std::is_integral_v<SubResult<T, T>>;

template <class T> using IterBase = RemoveReference<DereferenceResult<T>>;

template <class IT>
	requires(is_forward_iter<IT>)
auto distance(IT begin, IT end) {
	long long ret;
	if constexpr(is_random_iter<IT>) {
		PASSERT(!(end < begin));
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
