// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

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

	Span<llint, 2> values() { return v; }
	CSpan<llint, 2> values() const { return v; }

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

	Span<qint, 2> values() { return v; }
	CSpan<qint, 2> values() const { return v; }

	qint &x() { return v[0]; }
	qint &y() { return v[1]; }
	const qint &x() const { return v[0]; }
	const qint &y() const { return v[1]; }

	qint v[2];
};

template <class Scalar_> struct vector3 {
	using Scalar = Scalar_;
	enum { vector_size = 3 };

	constexpr vector3(Scalar x, Scalar y, Scalar z) : x(x), y(y), z(z) {}
	constexpr vector3(const float2 &xy, Scalar z) : x(xy.x), y(xy.y), z(z) {}
	constexpr vector3() : x(0.0f), y(0.0f), z(0.0f) {}

	explicit vector3(Scalar t) : x(t), y(t), z(t) {}
	template <class V, EnableIfVector<V, 3>...>
	explicit vector3(const V &vec) : x(vec[0]), y(vec[1]), z(vec[2]) {}
	explicit operator double3() const { return {double(x), double(y), double(z)}; }
	explicit operator int3() const { return {int(x), int(y), int(z)}; }

	vector3 operator*(const vector3 &rhs) const { return vector3(x * rhs.x, y * rhs.y, z * rhs.z); }
	vector3 operator/(const vector3 &rhs) const { return vector3(x / rhs.x, y / rhs.y, z / rhs.z); }
	vector3 operator+(const vector3 &rhs) const { return vector3(x + rhs.x, y + rhs.y, z + rhs.z); }
	vector3 operator-(const vector3 &rhs) const { return vector3(x - rhs.x, y - rhs.y, z - rhs.z); }
	vector3 operator*(Scalar s) const { return {x * s, y * s, z * s}; }
	vector3 operator/(Scalar s) const { return {x / s, y / s, z / s}; }
	vector3 operator-() const { return {-x, -y, -z}; }

	Scalar &operator[](int idx) { return v[idx]; }
	const Scalar &operator[](int idx) const { return v[idx]; }

	float2 xy() const { return {x, y}; }
	float2 xz() const { return {x, z}; }
	float2 yz() const { return {y, z}; }

	FWK_ORDER_BY(vector3, x, y, z)

	Span<Scalar, 3> values() { return v; }
	CSpan<Scalar, 3> values() const { return v; }

	union {
		struct {
			Scalar x, y, z;
		};
		Scalar v[3];
	};
};

using short3 = vector3<short>;
using llint3 = vector3<llint>;
using qint3 = vector3<qint>;

namespace detail {
	template <> struct IsIntegral<qint> : public std::true_type {};
	template <> struct MakeVector<llint, 2> { using type = llint2; };
	template <> struct MakeVector<qint, 2> { using type = qint2; };
	template <> struct MakeVector<short, 3> { using type = short3; };
	template <> struct MakeVector<llint, 3> { using type = llint3; };
	template <> struct MakeVector<qint, 3> { using type = qint3; };

	template <class T, class Enable = void> struct PromoteIntegral {
	  private:
		static auto resolve() {
			if
				constexpr(isVector<T>()) {
					using Promoted = typename PromoteIntegral<typename T::Scalar>::type;
					return fwk::MakeVector<Promoted, T::vector_size>();
				}
			else
				return T();
		}

	  public:
		using type = decltype(resolve());
	};

#define PROMOTION(base, promoted)                                                                  \
	template <> struct PromoteIntegral<base> { using type = promoted; };

	PROMOTION(short, int);
	PROMOTION(int, llint);
	PROMOTION(llint, qint);

#undef PROMOTION
}

template <class T> using PromoteIntegral = typename detail::PromoteIntegral<T>::type;

namespace xml_conversions {
	namespace detail {
		void toString(llint, TextFormatter &out);
		void toString(const llint2 &, TextFormatter &out);
		void toString(qint, TextFormatter &out);
		void toString(const qint2 &, TextFormatter &out);
	}
}
}

#endif
