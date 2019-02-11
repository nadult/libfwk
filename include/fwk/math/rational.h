// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/constants.h"

namespace fwk {

#ifdef FWK_CHECK_NANS
#define CHECK_NAN() DASSERT(!isNan())
#else
#define CHECK_NAN()
#endif

#define IF_SCALAR template <class U = T, EnableIfScalar<U>...>
#define IF_VEC template <class U = T, EnableIfVec<U>...>
#define IF_VEC3 template <class U = T, EnableIfVec<U, 3>...>

struct NoSignCheck {};
static constexpr NoSignCheck no_sign_check;

// Warning: these operations are far from optimal, if you know the numbers then
// you can perform computations using less operations and bits;
template <class T> struct Rational {
	static_assert(is_scalar<T> || is_vec<T>);

	static constexpr int vec_size = is_vec<T> ? fwk::vec_size<T> : 0;
	static_assert(isOneOf(vec_size, 0, 2, 3));
	using TBase = Conditional<is_vec<T>, VecScalar<T>, T>; // TODO: -> Base
	static_assert(is_integral<TBase> || is_ext24<TBase> || is_ext24<T>);

	using Scalar = Rational<TBase>;

	// TODO: paranoid overflow checks ?
	constexpr Rational(RealConstant<NumberType::infinity>) : m_num(1), m_den(0) {}
	constexpr Rational(const T &num, TBase den, NoSignCheck) : m_num(num), m_den(den) {
		CHECK_NAN();
	}
	constexpr Rational(const T &num, TBase den) : m_num(num), m_den(den) {
		if(den < TBase(0)) {
			m_num = -m_num;
			m_den = -m_den;
		}
		CHECK_NAN();
	}
	constexpr Rational() : m_num(0), m_den(1) {}

	template <class U, EnableIf<precise_conversion<U, T>>...>
	constexpr Rational(const Rational<U> &rhs) : Rational(T(rhs.num()), TBase(rhs.den())) {}
	template <class U, EnableIf<!precise_conversion<U, T>>...>
	explicit constexpr Rational(const Rational<U> &rhs)
		: Rational(T(rhs.num()), TBase(rhs.den())) {}

	template <class U, class UDen = TBase,
			  EnableIf<is_convertible<U, T> && precise_conversion<U, T>>...>
	constexpr Rational(const U &num, UDen den = TBase(1)) : Rational(T(num), TBase(den)) {}
	template <class U, class UDen = TBase,
			  EnableIf<is_constructible<U, T> && !precise_conversion<U, T>>...>
	constexpr explicit Rational(const U &num, UDen den = TBase(1)) : Rational(T(num), TBase(den)) {}

	template <class RT, EnableIf<is_real<RT> && vec_size == 0>...> explicit operator RT() const {
		return RT(m_num) / RT(m_den);
	}

	template <class RVec, class RT = VecScalar<RVec>,
			  EnableIf<is_real<RT> && fwk::vec_size<RVec> == vec_size>...>
	explicit operator RVec() const {
		return RVec(m_num) / RT(m_den);
	}

	Rational operator+(const Rational &) const;
	Rational operator-(const Rational &) const;
	Rational operator*(const Rational &)const;

	IF_VEC Rational operator*(const Scalar &s) const {
		return {m_num * s.num(), m_den * s.den(), no_sign_check};
	}
	IF_VEC Rational operator/(const Scalar &s) const {
		return {(s.num() < 0 ? -m_num : m_num) * s.den(), m_den * fwk::abs(s.num()), no_asserts};
	}

	template <class U, EnableIf<precise_conversion<U, TBase>>...> Rational operator*(U s) const {
		return {m_num * s, m_den, no_sign_check};
	}

	template <class U, EnableIf<precise_conversion<U, TBase>>...> Rational operator/(U s) const {
		return {s < 0 ? -m_num : m_num, m_den * fwk::abs(s), no_sign_check};
	}

	Rational operator-() const { return {-m_num, m_den, no_sign_check}; }

	IF_SCALAR int order(const Rational &) const;
	bool operator==(const Rational &) const;
	bool operator<(const Rational &) const;

	void operator>>(TextFormatter &) const;

	Rational normalized() const;

	// TODO: Shouldn't operations support these special states?
	IF_SCALAR bool isInfinity() const { return m_num != 0 && m_den == 0; }
	bool isNan() const {
		if constexpr(vec_size == 0)
			return m_num == 0 && m_den == 0;
		else if constexpr(vec_size > 0)
			return anyOf(m_num.values(), 0) && m_den == 0;
	}

	IF_SCALAR bool isNegative() const { return m_num < 0; }

	TBase den() const { return m_den; }
	const T &num() const { return m_num; }
	IF_VEC TBase num(int idx) const { return m_num[idx]; }

	IF_VEC Scalar operator[](int idx) const { return {m_num[idx], m_den, no_sign_check}; }
	IF_VEC Scalar x() const { return {m_num.x, m_den, no_sign_check}; }
	IF_VEC Scalar y() const { return {m_num.y, m_den, no_sign_check}; }
	IF_VEC3 Scalar z() const { return {m_num.z, m_den, no_sign_check}; }

	IF_VEC TBase numX() const { return m_num.x; }
	IF_VEC TBase numY() const { return m_num.y; }
	IF_VEC3 TBase numZ() const { return m_num.z; }

	IF_VEC3 Rational2<T> xy() const { return {m_num.xy(), m_den, no_sign_check}; }
	IF_VEC3 Rational2<T> xz() const { return {m_num.xz(), m_den, no_sign_check}; }
	IF_VEC3 Rational2<T> yz() const { return {m_num.yz(), m_den, no_sign_check}; }

	// TODO: its a mess with all different types
#define LEFT_SCALAR                                                                                \
	template <class U, EnableIf<(is_scalar<U> && !is_rational<U>) || is_vec<U>>...> friend

	// TODO: promotion to bigger type?
	LEFT_SCALAR bool operator<(const U &l, const Rational &r) { return Rational<U>(l) < r; }
	LEFT_SCALAR bool operator==(const U &l, const Rational &r) { return Rational<U>(l) == r; }

	LEFT_SCALAR auto operator+(const U &lhs, const Rational &rhs) { return Rational<U>(lhs) + rhs; }
	LEFT_SCALAR auto operator-(const U &lhs, const Rational &rhs) { return Rational<U>(lhs) - rhs; }
	LEFT_SCALAR auto operator*(const U &lhs, const Rational &rhs) { return Rational<U>(lhs) * rhs; }
	LEFT_SCALAR auto operator/(const U &lhs, const Rational &rhs) { return Rational<U>(lhs) / rhs; }
#undef LEFT_SCALAR

  private:
	T m_num;
	TBase m_den;
};

template <class T, class U>
static constexpr bool precise_conversion<T, Rational<U>> = precise_conversion<T, U>;
template <class T, class U>
static constexpr bool precise_conversion<Rational<T>, Rational<U>> = precise_conversion<T, U>;

Rational<int> rationalApprox(double value, int max_num, bool upper_bound);

#undef IF_SCALAR
#undef IF_VEC
#undef IF_VEC3
#undef CHECK_NAN

template <class T> T floor(const Rational<T> &value) {
	return ratioFloor(value.num(), value.den());
}
template <class T> T ceil(const Rational<T> &value) { return ratioCeil(value.num(), value.den()); }
template <class T> Rational<T> abs(const Rational<T> &value) {
	return {value.num() < 0 ? -value.num() : value.num(), value.den()};
}

// Nonstandard behaviour: 0.5 -> 1, -0.5 -> 0
// Basically it's equal to: floor(v + 1/2)
template <class T> T round(const Rational<T> &value) {
	if(value.den() & 1)
		return floor(Rational<T>{value.num() + (value.den() >> 1), value.den()});
	else
		return floor(Rational<T>{value.num() * 2 + value.den(), value.den() * 2});
}

template <class T, EnableIfScalar<T>...> auto divide(const T &num, const T &den) {
	if constexpr(is_integral<T> || is_ext24<T>)
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
	if constexpr(is_rational<T>) {
		using S = typename T::TBase;
		return value.num() < S(0) ? T(0) : value.num() > S(1) ? T(1) : value;
	} else
		return clamp(value, T(0), T(1));
}
}
