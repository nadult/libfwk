// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/math_base.h"

namespace fwk {

#ifdef FWK_CHECK_NANS
#define CHECK_NANS(...)                                                                            \
	if constexpr(is_fpt<T>)                                                                        \
	DASSERT(!isNan(__VA_ARGS__))
#else
#define CHECK_NANS(...)
#endif

// Dual numbers (for automatic differentiation)
template <class T> struct Dual {
	Dual(T real, T dual) : real(real), dual(dual) { CHECK_NANS(real, dual); }
	Dual(T real) : real(real), dual(0) { CHECK_NANS(real); }
	Dual() : real(0), dual(0) {}

	Dual operator+(T v) const { return {real + v, dual}; }
	Dual operator-(T v) const { return {real - v, dual}; }
	Dual operator*(T v) const { return {real * v, dual * v}; }
	Dual operator/(T v) const { return {real / v, dual / v}; }

	Dual operator+(const Dual &rhs) const { return {real + rhs.real, dual + rhs.dual}; }
	Dual operator-(const Dual &rhs) const { return {real - rhs.real, dual - rhs.dual}; }

	Dual operator*(const Dual &rhs) const {
		return {real * rhs.real, real * rhs.dual + dual * rhs.real};
	}
	Dual operator/(const Dual &rhs) const {
		auto num = dual * rhs.real - real * rhs.dual;
		return {real / rhs.real, num / (rhs.real * rhs.real)};
	}

	T real, dual;
};

#undef CHECK_NANS

template <class T> Dual<T> operator+(T lhs, const Dual<T> &rhs) {
	return {rhs.real + lhs, rhs.dual};
}
template <class T> Dual<T> operator-(T lhs, const Dual<T> &rhs) {
	return {rhs.real - lhs, rhs.dual};
}
template <class T> Dual<T> operator*(T lhs, const Dual<T> &rhs) {
	return {rhs.real * lhs, rhs.dual * lhs};
}
template <class T> Dual<T> operator/(T lhs, const Dual<T> &rhs) {
	return {rhs.real / lhs, rhs.dual / lhs};
}

template <class T> Dual<T> sqrt(const Dual<T> &v) {
	auto tmp = std::sqrt(v.real);
	return {tmp, v.dual / (T(2) * tmp)};
}

template <class T> TextFormatter &operator<<(TextFormatter &out, const Dual<T> &value) {
	out("% %", value.real, value.dual);
	return out;
}
}
