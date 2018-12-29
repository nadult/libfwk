// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"

namespace fwk {

#ifdef FWK_CHECK_NANS
#define CHECK_NAN() DASSERT(m_den != 0 || m_num != 0);
#else
#define CHECK_NAN()
#endif

// Warning: these operations are far from optimal, if you know the numbers then
// you can perform computations using less operations and bits;
template <class T> struct Rational {
	static_assert(is_integral<T>);

	// TODO: paranoid overflow checks ?
	constexpr Rational(T num, T den, NoAssertsTag) : m_num(num), m_den(den) { CHECK_NAN(); }

	template <class TN1, class TN2 = T,
			  EnableIf<preciseConversion<TN1, T>() && preciseConversion<TN2, T>()>...>
	constexpr Rational(TN1 num, TN2 den = T(1)) : m_num(num), m_den(den) {
		if(den < T(0)) {
			m_num = -m_num;
			m_den = -m_den;
		}
		CHECK_NAN();
	}

	template <class TN1, class TN2 = T,
			  EnableIf<!preciseConversion<TN1, T>() || !preciseConversion<TN2, T>()>...>
	constexpr explicit Rational(TN1 num, TN2 den = T(1)) : Rational(T(num), T(den)) {}
	constexpr Rational() : m_num(0), m_den(1) {}

	template <class IT>
	explicit constexpr Rational(const Rational<IT> &rhs) : m_num(rhs.num()), m_den(rhs.den()) {
		CHECK_NAN();
	}

	template <class RT, EnableIfReal<RT>...> explicit operator RT() const {
		return RT(m_num) / RT(m_den);
	}

	Rational operator+(const Rational &) const;
	Rational operator-(const Rational &) const;
	Rational operator*(const Rational &)const;

	template <class IT, EnableIf<preciseConversion<IT, T>()>...> Rational operator*(IT s) const {
		return {m_num * s, m_den, no_asserts};
	}

	template <class IT, EnableIf<preciseConversion<IT, T>()>...> Rational operator/(IT s) const {
		return {s < 0 ? -m_num : m_num, m_den * fwk::abs(s), no_asserts};
	}

	Rational operator-() const { return {-m_num, m_den, no_asserts}; }

	void operator+=(const Rational &rhs) { *this = (*this + rhs); }
	void operator-=(const Rational &rhs) { *this = (*this - rhs); }
	void operator*=(const Rational &rhs) { *this = (*this * rhs); }

	bool operator==(const Rational<T> &rhs) const;
	bool operator<(const Rational<T> &rhs) const;

	bool operator!=(const Rational &rhs) const { return !(*this == rhs); }
	bool operator>(const Rational &rhs) const { return rhs < *this; }
	bool operator<=(const Rational &rhs) const { return !(rhs < *this); }
	bool operator>=(const Rational &rhs) const { return !(*this < rhs); }

	void operator>>(TextFormatter &) const;

	Rational normalized() const;

	// Should operations support these special cases?
	bool isInfinity() const { return m_den == 0; }
	bool isNan() const { return m_num == 0 && m_den == 0; }

	bool isNegative() const { return m_num < 0; }

	int order(const Rational &rhs) const;

	T den() const { return m_den; }
	T num() const { return m_num; }

  private:
	T m_num, m_den;
};

template <class T> struct constant<Rational<T>> {
	static Rational<T> inf() { return {1, 0}; }
};

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

#undef CHECK_NAN
}
