// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/rational.h"

namespace fwk {

#ifdef FWK_CHECK_NANS
#define CHECK_NANS() DASSERT(m_den != 0 || allOf(m_num.values(), [](auto s) { return s != 0; }));
#else
#define CHECK_NANS()
#endif

template <class T> struct Rational2 {
	static_assert(is_integral<T>);

	enum { vec_size = 2 };
	using Scalar = Rational<T>;
	using IScalar = T;
	using IVector = MakeVec<T, 2>;

	constexpr Rational2(const IVector &vec, T den, NoAssertsTag) : m_num(vec), m_den(den) {
		CHECK_NANS();
	} // TODO: no need to do it here problably

	constexpr Rational2(const IVector &vec, T den = 1) : m_num(vec), m_den(den) {
		if(den < 0) {
			m_num = -m_num;
			m_den = -m_den;
		}
		CHECK_NANS();
	}

	constexpr Rational2(T x, T y, T den = 1) : Rational2(IVector(x, y), den) {}

	template <class TVec2, EnableIfIntegralVec<TVec2, 2>...>
	explicit constexpr Rational2(const TVec2 &vec, T den = 1)
		: Rational2(T(vec[0]), T(vec[1]), den) {}

	template <class T1>
	explicit constexpr Rational2(const Rational2<T1> &rhs)
		: Rational2(IVector(rhs.num()), T(rhs.den())) {}

	constexpr Rational2() : m_num(), m_den(1) {}

	template <class TVec2, EnableIfRealVec<TVec2, 2>...> explicit operator TVec2() const {
		using RT = typename TVec2::Scalar;
		return {RT((*this)[0]), RT((*this)[1])};
	}

	Rational2 operator+(const Rational2 &) const;
	Rational2 operator-(const Rational2 &) const;
	Rational2 operator*(const Rational2 &)const;

	Rational2 operator*(Scalar s) const { return {m_num * s.num(), m_den * s.den(), no_asserts}; }
	Rational2 operator/(Scalar s) const {
		return {(s.num() < 0 ? -m_num : m_num) * s.den(), m_den * fwk::abs(s.num()), no_asserts};
	}
	Rational2 operator-() const { return {-m_num, m_den, no_asserts}; }

	Scalar operator[](int idx) const { return {m_num[idx], m_den, no_asserts}; }

	void operator>>(TextFormatter &) const;

	IVector num() const { return m_num; }
	T num(int idx) const { return m_num[idx]; }
	T den() const { return m_den; }

	Scalar x() const { return (*this)[0]; }
	Scalar y() const { return (*this)[1]; }
	T numX() const { return m_num[0]; }
	T numY() const { return m_num[1]; }

	Rational2 normalized() const;

	FWK_ORDER_BY(Rational2, m_num, m_den)

  private:
	IVector m_num;
	T m_den;
};

#undef CHECK_NANS
}
