// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/format.h"
#include "fwk/math/gcd.h"
#include "fwk/math/rational.h"

namespace fwk {

// Source: RTCD 11.5.2
template <class T> int Rational<T>::order(const Rational &rhs) const {
	using PInt = Promote<T>;
	if constexpr(!is_same<PInt, T>) {
		auto v1 = PInt(m_num) * rhs.m_den;
		auto v2 = PInt(rhs.m_num) * m_den;
		return v1 == v2 ? 0 : v1 < v2 ? -1 : 1;
	}

	auto a = m_num, b = m_den;
	auto c = rhs.m_num, d = rhs.m_den;

	if(a == c && b == d)
		return 0;

	if(c < 0) {
		b = -b;
		c = -c;
	}
	if(d < 0) { // TODO: this test is not required
		a = -a;
		d = -d;
	}
	if(a < 0 && b < 0) {
		auto old_a = a, old_b = b;
		a = c;
		b = d;
		c = -old_a;
		d = -old_b;
	}
	if(a < 0)
		return -1;
	if(b < 0)
		return 1;

	if(a > b) {
		if(c < d)
			return 1;
		auto old_a = a, old_b = b;
		a = d;
		b = c;
		c = old_b;
		d = old_a;
	}

	if(c > d)
		return -1;

	// Continued fraction expansion
	while(a && c) {
		auto m = d / c;
		auto n = b / a;

		if(m != n) {
			if(m < n)
				return -1;
			if(m > n)
				return 1;
		}

		auto old_a = a;
		auto old_b = b;
		a = d % c;
		b = c;
		c = old_b % old_a;
		d = old_a;
	}

	if(a == 0)
		return c == 0 ? 0 : -1;
	return 1;
}

template <class T> Rational<T> Rational<T>::operator+(const Rational<T> &rhs) const {
	if(m_den == rhs.m_den)
		return {m_num + rhs.m_num, m_den, no_asserts};
	return {m_num * rhs.m_den + rhs.m_num * m_den, m_den * rhs.m_den, no_asserts};
}

template <class T> Rational<T> Rational<T>::operator-(const Rational<T> &rhs) const {
	if(m_den == rhs.m_den)
		return {m_num - rhs.m_num, m_den, no_asserts};
	return {m_num * rhs.m_den - rhs.m_num * m_den, m_den * rhs.m_den, no_asserts};
}

template <class T> Rational<T> Rational<T>::operator*(const Rational<T> &rhs) const {
	return {m_num * rhs.m_num, m_den * rhs.m_den, no_asserts};
}

template <class T> bool Rational<T>::operator==(const Rational<T> &rhs) const {
	using PInt = Promote<T>;

	if constexpr(is_same<PInt, T>)
		return order(rhs) == 0;
	else {
		if((m_num < 0) != (rhs.m_num < 0))
			return false;
		return PInt(m_num) * rhs.m_den == PInt(rhs.m_num) * m_den;
	}
}

template <class T> void Rational<T>::operator>>(TextFormatter &out) const {
	out(out.isStructured() ? "%/%" : "% %", num(), den());
}

template <class T> bool Rational<T>::operator<(const Rational<T> &rhs) const {
	using PInt = Promote<T>;

	// TODO: is comparison with inifinity / nan working ?
	// TODO: report errors when nans are present?
	if constexpr(is_same<PInt, T>)
		return order(rhs) == -1;
	else {
		if((m_num > 0) != (rhs.m_num > 0))
			return m_num <= 0;
		return PInt(m_num) * rhs.m_den < PInt(rhs.m_num) * m_den;
	}
}

template <class T> Rational<T> Rational<T>::normalized() const {
	auto t = gcd(m_num, m_den);
	return t == 1 ? *this : Rational(m_num / t, m_den / t, no_asserts);
}

template struct Rational<int>;
template struct Rational<llint>;
template struct Rational<qint>;
}
