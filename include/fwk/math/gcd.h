// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/qint.h"
#include "fwk/math_base.h"
#include "fwk/vector.h"

namespace fwk {

int gcd(int, int);
llint gcd(llint, llint);
qint gcd(qint, qint);

// Returns pairs: (prime, power)
template <class T> vector<Pair<T, int>> extractPrimes(T);

template <c_range TRange, class T = RemoveConst<RangeBase<TRange>>>
	requires(c_integral<T>)
T gcd(const TRange &range) {
	if(empty(range))
		return 0;
	auto it = begin(range), it_end = end(range);
	T out = *it++;
	while(it != it_end && out != 1) {
		out = gcd(out, *it);
		it++;
	}
	return out;
}

template <c_integral T> T gcdEuclid(T a, T b) {
	while(true) {
		if(a == T(0))
			return b;
		b = b % a;

		if(b == T(0))
			return a;
		a = a % b;
	}
}
}
