/* Copyright (C) 2015 - 2017 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#ifndef FWK_MATH_EXT_H
#define FWK_MATH_EXT_H

#include "fwk_math.h"

#ifndef __x86_64
#define FWK_USE_BOOST_MPC_INT
#endif

#ifdef FWK_USE_BOOST_MPC_INT
#include <boost/multiprecision/cpp_int.hpp>
#endif

namespace fwk {

using llint = long long;

#ifdef FWK_USE_BOOST_MPC_INT
using qint = boost::multiprecision::int128_t;
#else
using qint = __int128_t;
#endif

struct llint2 {
	using Scalar = llint;
	enum { vector_size = 2 };

	constexpr llint2(short2 rhs) : x(rhs.x), y(rhs.y) {}
	constexpr llint2(int2 rhs) : x(rhs.x), y(rhs.y) {}
	constexpr llint2(llint x, llint y) : x(x), y(y) {}
	constexpr llint2() : x(0), y(0) {}

	explicit llint2(llint t) : x(t), y(t) {}
	explicit operator short2() const { return short2(x, y); }
	explicit operator int2() const { return int2(x, y); }

	llint2 operator+(const llint2 &rhs) const { return {x + rhs.x, y + rhs.y}; }
	llint2 operator-(const llint2 &rhs) const { return {x - rhs.x, y - rhs.y}; }
	llint2 operator*(const llint2 &rhs) const { return {x * rhs.x, y * rhs.y}; }
	llint2 operator*(int s) const { return {x * s, y * s}; }
	llint2 operator/(int s) const { return {x / s, y / s}; }
	llint2 operator%(int s) const { return {x % s, y % s}; }
	llint2 operator-() const { return {-x, -y}; }

	llint &operator[](int idx) { return v[idx]; }
	const llint &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(llint2, x, y)
	FWK_VEC_RANGE()

	union {
		struct {
			llint x, y;
		};
		llint v[2];
	};
};

struct qint2 {
	using Scalar = qint;
	enum { vector_size = 2 };

	constexpr qint2(short2 rhs) : v{rhs.x, rhs.y} {}
	constexpr qint2(int2 rhs) : v{rhs.x, rhs.y} {}
	constexpr qint2(llint2 rhs) : v{rhs.x, rhs.y} {}
	constexpr qint2(qint x, qint y) : v{x, y} {}
	constexpr qint2() : v{0, 0} {}

	explicit qint2(qint t) : v{t, t} {}
	explicit operator short2() const { return short2((int)v[0], (int)v[1]); }
	explicit operator int2() const { return int2((int)v[0], (int)v[1]); }
	explicit operator llint2() const { return llint2((long long)v[0], (long long)v[1]); }

	qint2 operator+(const qint2 &rhs) const { return {v[0] + rhs[0], v[1] + rhs[1]}; }
	qint2 operator-(const qint2 &rhs) const { return {v[0] - rhs[0], v[1] - rhs[1]}; }
	qint2 operator*(const qint2 &rhs) const { return {v[0] * rhs[0], v[1] * rhs[1]}; }
	qint2 operator*(qint s) const { return {v[0] * s, v[1] * s}; }
	qint2 operator/(qint s) const { return {v[0] / s, v[1] / s}; }
	qint2 operator%(qint s) const { return {v[0] % s, v[1] % s}; }
	qint2 operator-() const { return {-v[0], -v[1]}; }

	qint &operator[](int idx) { return v[idx]; }
	const qint &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(qint2, v[0], v[1])
	FWK_VEC_RANGE()

	qint &x() { return v[0]; }
	qint &y() { return v[1]; }
	const qint &x() const { return v[0]; }
	const qint &y() const { return v[1]; }

	qint v[2];
};

namespace detail {
	template <> struct IsIntegral<qint> : public std::true_type {};
	template <> struct MakeVector<llint, 2> { using type = llint2; };
	template <> struct MakeVector<qint, 2> { using type = qint2; };
}

namespace xml_conversions {
	namespace detail {
		void toString(qint value, TextFormatter &out);
		void toString(qint2 value, TextFormatter &out);
	}
}
}

#endif
