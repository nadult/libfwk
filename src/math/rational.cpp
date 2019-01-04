// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/format.h"
#include "fwk/math/gcd.h"
#include "fwk/math/rational.h"

namespace fwk {

// Source: RTCD 11.5.2
template <class T> static int order(T lnum, T lden, T rnum, T rden) {
	// Handling infinities
	if(lden == 0 || rden == 0) {
		int lval = lden == 0 ? lnum < 0 ? -1 : 1 : 0;
		int rval = rden == 0 ? rnum < 0 ? -1 : 1 : 0;
		return lval < rval ? -1 : lval > rval ? 1 : 0;
	}

	using PInt = Promote<T>;
	if constexpr(!is_same<PInt, T>) {
		auto v1 = PInt(lnum) * rden;
		auto v2 = PInt(rnum) * lden;
		return v1 == v2 ? 0 : v1 < v2 ? -1 : 1;
	}

	auto a = lnum, b = lden;
	auto c = rnum, d = rden;

	if(a == c && b == d)
		return 0;

	if(c < 0) {
		b = -b;
		c = -c;
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

template <class T> Rational<T> Rational<T>::operator+(const Rational &rhs) const {
	if(m_den == rhs.m_den)
		return {m_num + rhs.m_num, TBase(m_den), no_sign_check};
	return {m_num * rhs.m_den + rhs.m_num * m_den, TBase(m_den * rhs.m_den), no_sign_check};
}

template <class T> Rational<T> Rational<T>::operator-(const Rational &rhs) const {
	if(m_den == rhs.m_den)
		return {m_num - rhs.m_num, TBase(m_den), no_sign_check};
	return {m_num * rhs.m_den - rhs.m_num * m_den, TBase(m_den * rhs.m_den), no_sign_check};
}

template <class T> Rational<T> Rational<T>::operator*(const Rational &rhs) const {
	return {m_num * rhs.m_num, TBase(m_den * rhs.m_den), no_sign_check};
}

template <class T>
template <class U, EnableIfScalar<U>...>
int Rational<T>::order(const Rational &rhs) const {
	return fwk::order(m_num, m_den, rhs.m_num, rhs.m_den);
}

template <class T> bool Rational<T>::operator==(const Rational &rhs) const {
	if constexpr(vec_size == 0)
		return order(rhs) == 0;
	else if constexpr(vec_size > 0) {
		// TODO: sometimes we want different kind of comparison: i.e. when one number
		//       is normalized and the other one is not then they are different
		for(int n = 0; n < vec_size; n++)
			if((*this)[n].order(rhs[n]) != 0)
				return false;
		return true;
	}
}

template <class T> void Rational<T>::operator>>(TextFormatter &out) const {
	out(out.isStructured() ? "%/%" : "% %", num(), den());
}

template <class T> bool Rational<T>::operator<(const Rational &rhs) const {
	if constexpr(vec_size == 0) {
		return order(rhs) == -1;
	} else if constexpr(vec_size > 0) {
		// TODO: sometimes we want different kind of comparison: i.e. when one number
		//       is normalized and the other one is not then they are different
		for(int n = 0; n < vec_size; n++)
			if(int cmp = (*this)[n].order(rhs[n]))
				return cmp == -1;
		return false;
	}
}

template <class T> Rational<T> Rational<T>::normalized() const {
	TBase t;
	if constexpr(vec_size == 0)
		t = gcd(m_num, m_den);
	else if constexpr(vec_size == 2)
		t = gcd(cspan({m_num[0], m_num[1], m_den}));
	else if constexpr(vec_size == 3)
		t = gcd(cspan({m_num[0], m_num[1], m_num[2], m_den}));
	return t == 1 ? *this : Rational(m_num / t, m_den / t, no_sign_check);
}

template struct Rational<short>;
template struct Rational<int>;
template struct Rational<llint>;
template struct Rational<qint>;

template int Rational<short>::order(const Rational &) const;
template int Rational<int>::order(const Rational &) const;
template int Rational<llint>::order(const Rational &) const;
template int Rational<qint>::order(const Rational &) const;

template struct Rational<short2>;
template struct Rational<int2>;
template struct Rational<llint2>;
template struct Rational<qint2>;

template struct Rational<short3>;
template struct Rational<int3>;
template struct Rational<llint3>;
template struct Rational<qint3>;
}
