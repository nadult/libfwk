// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/rational.h" // TODO: fix circular dependency
#include "fwk/math_base.h"

namespace fwk {

// Allows exact representation of a number of form:
// a + b * sqrt(2) + c * sqrt(3) + d * sqrt(6)
// where a, b, c, d are integers
// This is enough to represent any rotation which is a multiple of 15 degrees (there are 24 total)
//
// Segment intersection based on Ext24<int> is about 2x-3x slower than with integers and about 5x slower than with floats
// CGAL's CORE::Real is much slower though (about 100x, 1000x with conversion of coords to doubles)
template <class T> struct alignas(16) Ext24 {
	// TODO: ogarnąć nazwenictwo
	using Base = T;
	using PT = Promote<T>;
	using PPT = Promote<T, 2>;

	static_assert(is_integral<T>);

	constexpr Ext24(T one, T sq2, T sq3, T sq6, u8 bits)
		: a(one), b(sq2), c(sq3), d(sq6), bits(bits) {}
	constexpr Ext24() : Ext24(0, 0, 0, 0, 0) {}
	constexpr Ext24(T integral) : a(integral), b(0), c(0), d(0), bits(1) {}
	constexpr Ext24(T one, T sq2, T sq3, T sq6) : a(one), b(sq2), c(sq3), d(sq6) { updateBits(); }

	template <class U, EnableIf<precise_conversion<U, T>>...>
	Ext24(const Ext24<U> &rhs) : a(rhs.a), b(rhs.b), c(rhs.c), d(rhs.d) {}
	template <class U, EnableIf<!precise_conversion<U, T>>...>
	explicit Ext24(const Ext24<U> &rhs) : a(rhs.a), b(rhs.b), c(rhs.c), d(rhs.d) {}

	Ext24 operator+(const Ext24 &rhs) const {
		return Ext24(a + rhs.a, b + rhs.b, c + rhs.c, d + rhs.d);
	}
	Ext24 operator-(const Ext24 &rhs) const {
		return Ext24(a - rhs.a, b - rhs.b, c - rhs.c, d - rhs.d);
	}
	Ext24 operator-() const { return Ext24(-a, -b, -c, -d, bits); }

	Ext24 operator*(T s) const { return Ext24(a * s, b * s, c * s, d * s, s == 0 ? 0 : bits); }
	Ext24 intDivide(T s) const { return PASSERT(s != 0), Ext24(a / s, b / s, c / s, d / s); }

	// Problemem jest ciągłe przerzucanie z rejestrów sse/avx do pamięci na poziomie funkcji
	Ext24 operator*(const Ext24 &rhs) const;

	// TODO: intDiv zamiast / ? podobnie w int2, etc. ?
	// Kiedy tworzymy rational a kiedy dzielimy całkowicie ?

	RatExt24<T> operator/(const Ext24 &rhs) const { return {*this, rhs}; }

	// Inverse with integral denominator; Warning: requires 4x as many bits
	RatExt24<PPT> intDenomInverse() const;

	explicit operator double() const;
	explicit operator float() const { return (float)(double)*this; }

	bool isIntegral() const { return (bits & ~1) == 0; }
	bool isReal() const { return !isIntegral(); }

	constexpr void updateBits() {
		bits = (a != 0 ? 1 : 0) | (b != 0 ? 2 : 0) | (c != 0 ? 4 : 0) | (d != 0 ? 8 : 0);
	}

	int sign() const;
	int compare(const Ext24 &rhs) const { return (*this - rhs).sign(); }

	bool operator==(const Ext24 &rhs) const;
	bool operator<(const Ext24 &rhs) const { return compare(rhs) == -1; }

	void operator>>(TextFormatter &fmt) const;

	const T &operator[](int idx) const { return PASSERT(idx >= 0 && idx < 4), v[idx]; }
	T &operator[](int idx) { return PASSERT(idx >= 0 && idx < 4), v[idx]; }

	union {
		struct {
			T a, b, c, d;
		};
		T v[4];
	};
	u8 bits; // TODO: od razu wyliczanie znaku ? nie zawsze potrzenujemy...
};

// TODO: handle it properly? but how? its getting complicated
template <class T, class U>
static constexpr bool precise_conversion<T, Ext24<U>> = is_integral<T> &&precise_conversion<T, U>;
template <class T, class U>
static constexpr bool precise_conversion<Ext24<T>, Ext24<U>> = precise_conversion<T, U>;

// TODO: static or not?
template <class T> static constexpr Ext24<T> ext_sqrt2(0, 1, 0, 0);
template <class T> static constexpr Ext24<T> ext_sqrt3(0, 0, 1, 0);
template <class T> static constexpr Ext24<T> ext_sqrt6(0, 0, 0, 1);

template <class T> Ext24<T> ext24(T v) { return Ext24<T>(v); }
}
