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

#define IF_SCALAR template <class U = Num, EnableIf<fwk::dim<U> == 0>...>
#define IF_VEC template <class U = Num, EnableIf<fwk::dim<U> >= 2>...>
#define IF_VEC3 template <class U = Num, EnableIf<fwk::dim<U> == 3>...>

struct NoSignCheck {};
static constexpr NoSignCheck no_sign_check;

// Warning: these operations are far from optimal, if you know the numbers then
// you can perform computations using less operations and bits;
template <class T, int N> struct Rational {
	static_assert(is_integral<T> || is_ext24<T>);
	static constexpr int dim = N;

	using Scalar = Rational<T>;
	using Num = If<(N > 0), fwk::MakeVec<T, N>, T>;
	using Den = T;

	// TODO: paranoid overflow checks ?
	constexpr Rational(RealConstant<NumberType::infinity>) : m_num(1), m_den(0) {}
	constexpr Rational(const Num &n, const T &d, NoSignCheck) : m_num(n), m_den(d) { CHECK_NAN(); }
	constexpr Rational(const Num &n, const T &d) : m_num(n), m_den(d) {
		if(d < T(0)) {
			m_num = -m_num;
			m_den = -m_den;
		}
		CHECK_NAN();
	}
	constexpr Rational() : m_num(0), m_den(1) {}

	template <class U, EnableIf<precise_conversion<U, T>>...>
	constexpr Rational(const Rational<U, N> &rhs) : Rational(Num(rhs.num()), T(rhs.den())) {}
	template <class U, EnableIf<!precise_conversion<U, T>>...>
	explicit constexpr Rational(const Rational<U, N> &rhs)
		: Rational(Num(rhs.num()), T(rhs.den())) {}

	template <class UNum = Num, class UDen = T,
			  EnableIf<precise_conversion<Base<UNum>, T> && precise_conversion<UDen, T>>...>
	constexpr Rational(const UNum &num, const UDen &den = T(1)) : Rational(Num(num), T(den)) {}
	template <class UNum = Num, class UDen = T,
			  EnableIf<!precise_conversion<Base<UNum>, T> || !precise_conversion<UDen, T>>...>
	constexpr explicit Rational(const UNum &num, const UDen &den = T(1))
		: Rational(Num(num), T(den)) {}

	template <class RT, EnableIf<is_fpt<Base<RT>>>...> explicit operator RT() const {
		return RT(m_num) / Base<RT>(m_den);
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

	template <class U, EnableIf<precise_conversion<U, T>>...> Rational operator*(U s) const {
		return {m_num * s, m_den, no_sign_check};
	}

	template <class U, EnableIf<precise_conversion<U, T>>...> Rational operator/(U s) const {
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
		if constexpr(dim == 0)
			return m_num == 0 && m_den == 0;
		else
			return anyOf(m_num.values(), 0) && m_den == 0;
	}

	IF_SCALAR bool isNegative() const { return m_num < 0; }

	const Den &den() const { return m_den; }
	const Num &num() const { return m_num; }
	IF_VEC T num(int idx) const { return m_num[idx]; }

	llint hash() const;

	IF_VEC Scalar operator[](int idx) const { return {m_num[idx], m_den, no_sign_check}; }
	IF_VEC Scalar x() const { return {m_num.x, m_den, no_sign_check}; }
	IF_VEC Scalar y() const { return {m_num.y, m_den, no_sign_check}; }
	IF_VEC3 Scalar z() const { return {m_num.z, m_den, no_sign_check}; }

	IF_VEC T numX() const { return m_num.x; }
	IF_VEC T numY() const { return m_num.y; }
	IF_VEC3 T numZ() const { return m_num.z; }

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
	Num m_num;
	Den m_den;
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

// TODO: ratDivide
template <class T, class T1, EnableIf<dim<T> == 0>...> auto divide(const T &num, const T1 &den) {
	static_assert(precise_conversion<T1, T>);
	if constexpr(is_integral<T> || is_ext24<T>)
		return Rational<T>(num, den);
	else
		return num / den;
}

template <class T, class T1, EnableIf<(dim<T>> 0)>...> auto divide(const T &num, const T1 &den) {
	static_assert(precise_conversion<T1, Scalar<T>>);
	constexpr bool make_rational = is_integral<Scalar<T>> || is_ext24<Scalar<T>>;
	if constexpr(make_rational && dim<T> == 2)
		return Rational2<T>(num, den);
	else if constexpr(make_rational && dim<T> == 3)
		return Rational3<T>(num, den);
	else
		return num / den;
}

template <class T, EnableIf<is_scalar<T>>...> auto clamp01(const T &value) {
	if constexpr(is_rational<T>) {
		using S = typename T::Num;
		return value.num() < S(0) ? T(0) : value.num() > S(1) ? T(1) : value;
	} else
		return clamp(value, T(0), T(1));
}
}
