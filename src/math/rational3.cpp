// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/format.h"
#include "fwk/math/gcd.h"
#include "fwk/math/rational3.h"

namespace fwk {

template <class T> Rational3<T> Rational3<T>::operator+(const Rational3<T> &rhs) const {
	if(m_den == rhs.m_den)
		return {m_num + rhs.m_num, m_den};
	return {m_num * rhs.m_den + rhs.m_num * m_den, m_den * rhs.m_den, no_asserts};
}

template <class T> Rational3<T> Rational3<T>::operator-(const Rational3<T> &rhs) const {
	if(m_den == rhs.m_den)
		return {m_num - rhs.m_num, m_den};
	return {m_num * rhs.m_den - rhs.m_num * m_den, m_den * rhs.m_den, no_asserts};
}

template <class T> Rational3<T> Rational3<T>::operator*(const Rational3<T> &rhs) const {
	return {m_num * rhs.m_num, m_den * rhs.m_den, no_asserts};
}

template <class T> Rational3<T> Rational3<T>::normalized() const {
	auto t = gcd<std::initializer_list<T>>({m_num[0], m_num[1], m_num[2], m_den});
	return t == 1 ? *this : Rational3<T>(m_num / t, m_den / t, no_asserts);
}

template <class T> void Rational3<T>::operator>>(TextFormatter &out) const {
	out(out.isStructured() ? "%/%" : "% %", num(), den());
}

template struct Rational3<int>;
template struct Rational3<llint>;
template struct Rational3<qint>;
}
