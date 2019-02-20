// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/rational.h" // TODO: fix circular dependency
#include "fwk/math_base.h"

namespace fwk {

// TODO: problem: często nie chcemy obliczać na rationalach, ale w konkretnej skali
// jak to sensownie obsługiwać?

// Allows exact representation of a number of form:
// a + b * sqrt(2) + c * sqrt(3) + d * sqrt(6)
// where a, b, c, d are integers
// This is enough to represent any rotation which is a multiple of 15 degrees (there are 24 total)
//
// Segment intersection based on Ext24<int> is about 2x-3x slower than with integers and about 5x slower than with floats
// CGAL's CORE::Real is much slower though (about 100x, 1000x with conversion of coords to doubles)
// TODO: it got slower after moving to libfwk, investigate why
template <class T> struct Ext24 {
	// TODO: ogarnąć nazwenictwo
	using Base = T;
	using PT = Promote<T>;
	using PPT = Promote<T, 2>;

	static_assert(is_integral<T>);

	constexpr Ext24() : Ext24(0, 0, 0, 0) {}
	constexpr Ext24(T integral) : a(integral), b(0), c(0), d(0) {}
	constexpr Ext24(T one, T sq2, T sq3, T sq6) : a(one), b(sq2), c(sq3), d(sq6) {}

	template <class U, EnableIf<precise_conversion<U, T>>...>
	Ext24(const Ext24<U> &rhs) : a(rhs.a), b(rhs.b), c(rhs.c), d(rhs.d) {}
	template <class U, EnableIf<!precise_conversion<U, T>>...>
	explicit Ext24(const Ext24<U> &rhs) : a(rhs.a), b(rhs.b), c(rhs.c), d(rhs.d) {}

	constexpr Ext24 operator+(const Ext24 &rhs) const {
		return Ext24(a + rhs.a, b + rhs.b, c + rhs.c, d + rhs.d);
	}
	constexpr Ext24 operator-(const Ext24 &rhs) const {
		return Ext24(a - rhs.a, b - rhs.b, c - rhs.c, d - rhs.d);
	}
	constexpr Ext24 operator-() const { return Ext24(-a, -b, -c, -d); }

	Ext24 operator*(T s) const { return Ext24(a * s, b * s, c * s, d * s); }
	Ext24 operator/(T s) const { return PASSERT(s != 0), Ext24(a / s, b / s, c / s, d / s); }

	// Problemem jest ciągłe przerzucanie z rejestrów sse/avx do pamięci na poziomie funkcji
	Ext24 operator*(const Ext24 &rhs) const;

	// Inverse with integral denominator; Warning: requires 4x as many bits
	RatExt24<PPT> intDenomInverse() const;

	explicit operator double() const;
	explicit operator T() const;
	explicit operator float() const { return (float)(double)*this; }

	T asIntegral() const { return a; }
	T gcd() const;
	llint hash() const;

	bool isIntegral() const { return b == 0 && c == 0 && d == 0; }
	bool isReal() const { return !isIntegral(); }

	int sign() const;
	int compare(const Ext24 &rhs) const { return (*this - rhs).sign(); }

	bool operator==(const Ext24 &rhs) const;
	bool operator<(const Ext24 &rhs) const { return compare(rhs) == -1; }

	void operator>>(TextFormatter &fmt) const;

	const T &operator[](int idx) const { return PASSERT(idx >= 0 && idx < 4), v[idx]; }
	T &operator[](int idx) { return PASSERT(idx >= 0 && idx < 4), v[idx]; }

#define LEFT_SCALAR template <class U, EnableIf<is_constructible<Ext24, U>>...> friend

	LEFT_SCALAR bool operator<(const U &l, const Ext24 &r) { return Ext24{l} < r; }
	LEFT_SCALAR bool operator==(const U &l, const Ext24 &r) { return Ext24{l} == r; }
	LEFT_SCALAR auto operator*(const U &l, const Ext24 &r) { return Ext24{l} * r; }

#undef LEFT_SCALAR

	union {
		struct {
			T a, b, c, d;
		};
		T v[4];
	};
};

template <class T> Maybe<int> vectorToAngle(const Rat2Ext24<T> &);

// Angles must be a multiply of 15
template <class T> Rat2Ext24<T> rotateVector(const Rat2Ext24<T> &, int degrees);
Rat2Ext24<short> angleToVectorExt24(int degrees, int scale = 1);

// TODO: static or not?
template <class T> static constexpr Ext24<T> ext_sqrt2(0, 1, 0, 0);
template <class T> static constexpr Ext24<T> ext_sqrt3(0, 0, 1, 0);
template <class T> static constexpr Ext24<T> ext_sqrt6(0, 0, 0, 1);

template <class T> Ext24<T> ext24(T v) { return Ext24<T>(v); }
}
