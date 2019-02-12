// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/constants.h"
#include "fwk/maybe.h"

namespace fwk {

template <class T> struct Interval {
	static_assert(is_scalar<T>, "");

	Interval(T min, T max) : min(min), max(max) {
#ifdef FWK_CHECK_NANS
		if constexpr(is_real<T>)
			DASSERT(!isnan(min) && !isnan(max));
#endif
	}

	Interval(const Pair<T> &pair) : Interval(pair.first, pair.second) {}
	explicit Interval(T point) : Interval(point, point) {}
	Interval() : min(0), max(0) {}

	Interval operator*(T val) const {
		return val < T(0) ? Interval(max * val, min * val) : Interval(min * val, max * val);
	}
	Interval operator/(T val) const { return (*this) * (T(1) / val); }
	Interval operator+(T val) const { return Interval(min + val, max + val); }
	Interval operator-(T val) const { return Interval(min - val, max - val); }

	template <class U = T, EnableIfReal<U>...> static Interval inf() { return {fwk::inf}; }

	T size() const { return max - min; }
	bool valid() const { return min <= max; }
	bool empty() const { return !(max > min); }

	Maybe<Interval> isect(const Interval &rhs) const {
		if(min > rhs.max || rhs.max < min)
			return none;
		return Interval{fwk::max(min, rhs.min), fwk::min(max, rhs.max)};
	}

	Interval enclose(const Interval &rhs) const {
		return {fwk::min(min, rhs.min), fwk::max(max, rhs.max)};
	}

	Interval enclose(T point) const { return {fwk::min(min, point), fwk::max(max, point)}; }

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(Interval, min, max);

	T min, max;
};
}
