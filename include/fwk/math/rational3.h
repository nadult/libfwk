// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/rational2.h"

namespace fwk {

#ifdef FWK_CHECK_NANS
#define CHECK_NANS() DASSERT(m_den != 0 || allOf(m_num.values(), [](auto s) { return s != 0; }));
#else
#define CHECK_NANS()
#endif

// Rational -> Ratio ?
// Add funcs ratioFloor, ratioCeil, etc ?
template <class T> struct Rational3 {
	static_assert(isIntegral<T>());

	enum { vec_size = 3 };
	using Scalar = Rational<T>;
	using IScalar = T;
	using IVector = MakeVec<T, 3>;

	constexpr Rational3(const IVector &vec, T den, NoAssertsTag) : m_num(vec), m_den(den) {
		CHECK_NANS();
	}
	constexpr Rational3(const IVector &vec, T den = 1) : m_num(vec), m_den(den) {
		if(den < 0) {
			m_num = -m_num;
			m_den = -m_den;
		}
		CHECK_NANS();
	}

	constexpr Rational3(T x, T y, T z, T den = 1) : Rational3(IVector(x, y, z), den) {}

	template <class TVec3, EnableIfIntegralVec<TVec3, 3>...>
	explicit constexpr Rational3(const TVec3 &vec, T den = 1)
		: Rational3(T(vec[0]), T(vec[1]), T(vec[2]), den) {}

	template <class T1>
	explicit constexpr Rational3(const Rational3<T1> &rhs)
		: Rational3(IVector(rhs.num()), T(rhs.den())) {}

	constexpr Rational3() : m_num(), m_den(1) {}

	template <class TVec3, EnableIfRealVec<TVec3, 3>...> explicit operator TVec3() const {
		using RT = typename TVec3::Scalar;
		return {RT((*this)[0]), RT((*this)[1]), RT((*this)[2])};
	}

	Rational3 operator+(const Rational3 &) const;
	Rational3 operator-(const Rational3 &) const;
	Rational3 operator*(const Rational3 &)const;

	Rational3 operator*(Scalar s) const { return {m_num * s.num(), m_den * s.den(), no_asserts}; }
	Rational3 operator/(Scalar s) const {
		return {(s.num() < 0 ? -m_num : m_num) * s.den(), m_den * fwk::abs(s.num()), no_asserts};
	}
	Rational3 operator-() const { return {-m_num, m_den, no_asserts}; }

	Scalar operator[](int idx) const { return {m_num[idx], m_den, no_asserts}; }

	void operator>>(TextFormatter &) const;

	IVector num() const { return m_num; }
	T num(int idx) const { return m_num[idx]; }
	T den() const { return m_den; }

	Scalar x() const { return (*this)[0]; }
	Scalar y() const { return (*this)[1]; }
	Scalar z() const { return (*this)[2]; }
	T numX() const { return m_num[0]; }
	T numY() const { return m_num[1]; }
	T numZ() const { return m_num[2]; }

	Rational2<T> xy() const { return {m_num.xy(), m_den, no_asserts}; }
	Rational2<T> xz() const { return {m_num.xz(), m_den, no_asserts}; }
	Rational2<T> yz() const { return {m_num.yz(), m_den, no_asserts}; }

	Rational3 normalized() const;

	FWK_ORDER_BY(Rational3, m_num, m_den)

  private:
	IVector m_num;
	T m_den;
};

#undef CHECK_NANS
}
