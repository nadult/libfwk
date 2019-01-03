// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/rational.h"
#include "fwk/math/rational2.h"
#include "fwk/math/rational3.h"

namespace fwk {

template <class T, EnableIfScalar<T>...> auto divide(const T &num, const T &den) {
	if constexpr(is_integral<T>)
		return Rational<T>(num, den);
	else
		return num / den;
}

template <class TV, class T1, class T = VecScalar<TV>> auto divide(const TV &num, const T1 &den) {
	if constexpr(is_integral<T> && vec_size<TV> == 2)
		return Rational2<T>(num, den);
	else if constexpr(is_integral<T> && vec_size<TV> == 3)
		return Rational3<T>(num, den);
	else
		return num / den;
}

template <class T> auto clamp01(const T &value) {
	if constexpr(is_rational<T>)
		return value.num() < 0 ? T(0) : value.num() > 1 ? T(1) : value;
	else
		return clamp(value, T(0), T(1));
}

// TODO: merge rationals into single template; is_vec would have to be modified
}
