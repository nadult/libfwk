// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/light_tuple.h"
#include "fwk/sys_base.h"
#include "fwk_index_range.h"
#include <cmath>

#ifndef __x86_64
#define FWK_USE_BOOST_MPC_INT
#endif

#ifdef FWK_USE_BOOST_MPC_INT
#include <boost/multiprecision/cpp_int.hpp>
#endif

namespace fwk {

template <int N> struct NotAValidVec;

struct NotFloatingPoint;
struct NotIntegral;

// -------------------------------------------------------------------------------------------
// ---  Type forward declarations  -----------------------------------------------------------

template <class T> struct vec2;
template <class T> struct vec3;
template <class T> struct vec4;

template <class T, int N = 0> struct Rational;
template <class T> using Rational2 = Rational<T, 2>;
template <class T> using Rational3 = Rational<T, 3>;

template <class T> struct Ext24;
template <class T> using RatExt24 = Rational<Ext24<T>>;
template <class T> using Rat2Ext24 = Rational<Ext24<T>, 2>;
template <class T> using Rat3Ext24 = Rational<Ext24<T>, 3>;

using llint = long long;
#ifdef FWK_USE_BOOST_MPC_INT
using qint = boost::multiprecision::int128_t;
#else
using qint = __int128_t;
#endif

using short2 = vec2<short>;
using short3 = vec3<short>;
using short4 = vec4<short>;
using int2 = vec2<int>;
using int3 = vec3<int>;
using int4 = vec4<int>;
using float2 = vec2<float>;
using float3 = vec3<float>;
using float4 = vec4<float>;
using double2 = vec2<double>;
using double3 = vec3<double>;
using double4 = vec4<double>;
using llint2 = vec2<llint>;
using llint3 = vec3<llint>;
using llint4 = vec4<llint>;
using qint2 = vec2<qint>;
using qint3 = vec3<qint>;
using qint4 = vec4<qint>;

template <class T> class Box;
template <class T> struct Interval;
template <class T> class IsectParam;

template <class T, int N> class Triangle;
template <class T, int N> class Plane;
template <class T, int N> class Ray;
template <class T, int N> class Segment;

template <class T> using Segment2 = Segment<T, 2>;
template <class T> using Segment3 = Segment<T, 3>;

template <class T> using Triangle2 = Triangle<T, 2>;
template <class T> using Triangle3 = Triangle<T, 3>;

template <class T> using Plane2 = Plane<T, 2>;
template <class T> using Plane3 = Plane<T, 3>;

template <class T> using Ray2 = Ray<T, 2>;
template <class T> using Ray3 = Ray<T, 3>;

template <class T> using Box2 = Box<vec2<T>>;
template <class T> using Box3 = Box<vec3<T>>;

// TODO:
// Triangle3f, Triangle3d or Triangle3D Triangle3F
using Triangle3F = Triangle<float, 3>;
using Triangle3D = Triangle<double, 3>;
using Triangle2F = Triangle<float, 2>;
using Triangle2D = Triangle<double, 2>;
using Plane3F = Plane<float, 3>;
using Plane3D = Plane<double, 3>;
using Plane2F = Plane<float, 2>;
using Plane2D = Plane<double, 2>;
using Segment3F = Segment<float, 3>;
using Segment3D = Segment<double, 3>;
using Segment2F = Segment<float, 2>;
using Segment2D = Segment<double, 2>;
using Ray3F = Ray<float, 3>;
using Ray3D = Ray<double, 3>;
using Ray2F = Ray<float, 2>;
using Ray2D = Ray<double, 2>;

using Segment3I = Segment<int, 3>;
using Segment2I = Segment<int, 2>;

using IRect = Box<int2>;
using FRect = Box<float2>;
using DRect = Box<double2>;

using IBox = Box<int3>;
using FBox = Box<float3>;
using DBox = Box<double3>;

class Matrix3;
class Matrix4;
class Cylinder;
class Frustum;
class Quat;
class AxisAngle;
class AffineTrans;
class Projection;
class Tetrahedron;
class Random;

// -------------------------------------------------------------------------------------------
// ---  Type information ---------------------------------------------------------------------

namespace detail {

	template <class T, int N> struct MakeVec { using Type = NotAValidVec<N>; };
	template <class T> struct MakeVec<T, 2> { using Type = vec2<T>; };
	template <class T> struct MakeVec<T, 3> { using Type = vec3<T>; };
	template <class T> struct MakeVec<T, 4> { using Type = vec4<T>; };

	template <class T> struct Scalar {
		using Type = If<is_one_of<T, Matrix3, Matrix4, Cylinder, Frustum, Quat, AxisAngle,
								  Projection, Tetrahedron>,
						float, T>;
	};
	template <class T> struct Scalar<vec2<T>> { using Type = T; };
	template <class T> struct Scalar<vec3<T>> { using Type = T; };
	template <class T> struct Scalar<vec4<T>> { using Type = T; };
	template <class T> struct Scalar<Interval<T>> { using Type = T; };
	template <class T> struct Scalar<IsectParam<T>> { using Type = T; };
	template <class T> struct Scalar<Box<T>> { using Type = typename Scalar<T>::Type; };
	template <class T, int N> struct Scalar<Triangle<T, N>> { using Type = T; };
	template <class T, int N> struct Scalar<Plane<T, N>> { using Type = T; };
	template <class T, int N> struct Scalar<Segment<T, N>> { using Type = T; };
	template <class T, int N> struct Scalar<Ray<T, N>> { using Type = T; };
	template <class T, int N> struct Scalar<Rational<T, N>> { using Type = Rational<T>; };

	template <class T> struct ScBase { using Type = T; };
	template <class T, int N> struct ScBase<Rational<T, N>> {
		using Type = typename ScBase<T>::Type;
	};
	template <class T> struct ScBase<Ext24<T>> { using Type = typename ScBase<T>::Type; };

	template <class T> struct RatSize { static constexpr int value = -1; };
	template <class T, int N> struct RatSize<Rational<T, N>> { static constexpr int value = N; };

	// TODO: should Base, dim work for Segment, Box, etc. ?
	// If it will then, we have to modify is_vec, etc.
	// If we had field, Base<> would be easy
}

template <class T> static constexpr int dim = 0;
template <class T> static constexpr int dim<vec2<T>> = 2;
template <class T> static constexpr int dim<vec3<T>> = 3;
template <class T> static constexpr int dim<vec4<T>> = 4;
template <class T, int N> static constexpr int dim<Rational<T, N>> = N;

template <class T>
static constexpr bool is_integral = std::is_integral<T>::value || is_same<T, qint>;
template <class T> static constexpr bool is_fpt = std::is_floating_point<T>::value;
template <class T> static constexpr bool is_fundamental = is_integral<T> || is_fpt<T>;
template <class T> static constexpr bool is_rational = detail::RatSize<T>::value == 0;

template <class T, int ReqN = 0>
static constexpr bool is_rational_vec = detail::RatSize<T>::value > 0 &&
										(ReqN == 0 || ReqN == detail::RatSize<T>::value);

template <class T> static constexpr bool is_ext24 = false;
template <class T> static constexpr bool is_ext24<Ext24<T>> = true;
template <class T, int ReqN = 0>
static constexpr bool is_vec = ((dim<T>) > 0) && !is_rational<T> && (ReqN == 0 || ReqN == dim<T>);

template <class T> static constexpr bool is_scalar = is_fundamental<T> || is_ext24<T>;
template <class T> static constexpr bool is_scalar<Rational<T, 0>> = true;

template <class T, int N> using MakeVec = typename detail::MakeVec<T, N>::Type;

template <class T> using Scalar = typename detail::Scalar<T>::Type;
template <class T> using Base = typename detail::ScBase<Scalar<T>>::Type;

template <class T> using EnableIfFpt = EnableIf<is_fpt<T>, NotFloatingPoint>;
template <class T> using EnableIfIntegral = EnableIf<is_integral<T>, NotIntegral>;

template <class T, int ReqN = 0> using EnableIfVec = EnableIf<is_vec<T, ReqN>, NotAValidVec<ReqN>>;
template <class T, int ReqN = 0>
using EnableIfFptVec = EnableIf<is_vec<T, ReqN> && is_fpt<Base<T>>, NotAValidVec<ReqN>>;
template <class T, int ReqN = 0>
using EnableIfIntegralVec = EnableIf<is_vec<T, ReqN> && is_integral<Base<T>>, NotAValidVec<ReqN>>;

template <class T> struct ToFpt { using type = double; };
template <> struct ToFpt<float> { using type = float; };
template <> struct ToFpt<short> { using type = float; };
template <> struct ToFpt<llint> { using type = long double; };

// -------------------------------------------------------------------------------------------
// ---  Specifying promotions & precise conversion rules -------------------------------------

namespace detail {
	template <class T> struct Promotion { using type = T; };

#define PROMOTION(base, promoted)                                                                  \
	template <> struct Promotion<base> { using type = promoted; };

	PROMOTION(short, int);
	PROMOTION(int, llint);
	PROMOTION(llint, qint);
	PROMOTION(float, double);
	PROMOTION(double, long double)

#undef PROMOTION

	template <class T, int count = 1> auto promote() {
		static_assert(count >= 0);
		if constexpr(count == 0)
			return DECLVAL(T);
		else {
			if constexpr(is_vec<T>) {
				using Promoted = decltype(promote<fwk::Scalar<T>, count>());
				return fwk::MakeVec<Promoted, dim<T>>();
			} else if constexpr(is_rational<T>) {
				using Promoted = decltype(promote<typename T::Den, count>());
				return Rational<Promoted, dim<T>>();
			} else if constexpr(is_ext24<T>) {
				using Promoted = decltype(promote<Base<T>, count>());
				return Ext24<Promoted>();
			} else {
				return promote<typename detail::Promotion<T>::type, count - 1>();
			}
		}
	}
}

// TODO: maybe Promote is not the best name?
// TODO: sometimes we want to raise an error if promotion is not available?
template <class T, int count = 1> using Promote = decltype(detail::promote<T, count>());
template <class T, int count = 1>
using PromoteIntegral = If<is_integral<Base<T>>, Promote<T, count>, T>;

template <class From, class To>
static constexpr bool precise_conversion = []() {
	if constexpr(is_same<From, To>)
		return true;
	else if constexpr(!is_same<Promote<From>, From>)
		return precise_conversion<Promote<From>, To>;
	return false;
}();

template <class T, class U>
static constexpr bool precise_conversion<T, Ext24<U>> = precise_conversion<T, U>;
template <class T, class U>
static constexpr bool precise_conversion<Ext24<T>, Ext24<U>> = precise_conversion<T, U>;

template <class T, class U>
static constexpr bool precise_conversion<T, Rational<U>> = precise_conversion<T, U>;
template <class T, class U, int N>
static constexpr bool precise_conversion<Rational<T, N>, Rational<U, N>> = precise_conversion<T, U>;

template <class T, class U>
static constexpr bool precise_conversion<vec2<T>, vec2<U>> = precise_conversion<T, U>;
template <class T, class U>
static constexpr bool precise_conversion<vec3<T>, vec3<U>> = precise_conversion<T, U>;
template <class T, class U>
static constexpr bool precise_conversion<vec4<T>, vec4<U>> = precise_conversion<T, U>;

template <class T, class U, int N>
static constexpr bool precise_conversion<T, Rational<U, N>> =
	dim<T> == N &&precise_conversion<Scalar<T>, U>;

#define PRECISE(from, to) template <> static constexpr bool precise_conversion<from, to> = true;

PRECISE(short, float)
PRECISE(int, double)
PRECISE(long long, long double)
PRECISE(float, double)
PRECISE(double, long double)
// TODO: full long doubles support?

#undef PRECISE

// -------------------------------------------------------------------------------------------
// ---  Basic math functions  ----------------------------------------------------------------

Pair<float> sincos(float radians);
Pair<double> sincos(double radians);

// TODO: why not templates?
inline int abs(int s) { return std::abs(s); }
inline long long abs(long long s) { return std::abs(s); }
inline double abs(double s) { return std::abs(s); }
inline float abs(float s) { return std::abs(s); }
inline long double abs(long double s) { return std::abs(s); }
template <class T, EnableIf<is_scalar<T>>...> T abs(T value) {
	return value < T(0) ? -value : value;
}

using std::ceil;
using std::floor;

// Nonstandard behaviour: rounding half up (0.5 -> 1, -0.5 -> 0)
template <class T, EnableIfFpt<T>...> T round(T value) { return floor(value + T(0.5)); }

template <class T, EnableIfFpt<T>...> inline T inv(T s) { return T(1) / s; }

template <class T, EnableIf<is_scalar<T>>...> T clamp(const T &v, const T &tmin, const T &tmax) {
	return min(tmax, max(tmin, v));
}

template <class T, class Scalar> inline T lerp(const T &a, const T &b, const Scalar &x) {
	return (b - a) * x + a;
}

template <class T, EnableIfIntegral<T>...> T ratioFloor(T value, T div) {
	return value < T(0) ? (value - div + T(1)) / div : value / div;
}

template <class T, EnableIfIntegral<T>...> T ratioCeil(T value, T div) {
	return value > T(0) ? (value + div - T(1)) / div : value / div;
}

template <class T>
static constexpr T epsilon = []() {
	static_assert(is_fpt<T>);
	// TODO: verify these values
	return is_same<T, float> ? T(1E-6f) : is_same<T, double> ? T(1E-14) : T(1E-18L);
}();

template <class T> bool isAlmostOne(T value) {
	auto diff = T(1) - value;
	return diff < epsilon<T> && diff > -epsilon<T>;
}

float frand();

template <class T, EnableIfIntegral<T>...> constexpr bool isPowerOfTwo(T value) {
	return (value & (value - 1)) == 0;
}

constexpr int countLeadingZeros(uint value) { return value ? __builtin_clz(value) : 32; }
constexpr int countLeadingZeros(u64 value) { return value ? __builtin_clzll(value) : 64; }
constexpr int countTrailingZeros(uint value) { return value ? __builtin_clz(value) : 32; }
constexpr int countTrailingZeros(u64 value) { return value ? __builtin_clzll(value) : 64; }
constexpr int countBits(uint value) { return __builtin_popcount(value); }
constexpr int countBits(u64 value) { return __builtin_popcountll(value); }

template <class T, EnableIfIntegral<T>...> T nextPow2(T val) {
	T out = 1;
	while(out < val)
		out *= 2;
	return out;
}

using std::isnan;
template <class T, class... TV> bool isnan(T v0, TV... vn) { return isnan(v0) || isnan(vn...); }
template <class T, EnableIfFptVec<T>...> bool isnan(const T &v) {
	return anyOf(v.values(), [](auto s) { return isnan(s); });
}
template <class TRange, class T = RangeBase<TRange>, EnableIf<is_fpt<Base<T>>>...>
bool isnan(const TRange &range) {
	return anyOf(range, [](auto s) { return isnan(s); });
}

// -------------------------------------------------------------------------------------------
// ---  Basic vector class templates  --------------------------------------------------------

#ifdef FWK_CHECK_NANS
#define CHECK_NANS(...)                                                                            \
	if constexpr(is_fpt<T>)                                                                        \
	DASSERT(!isnan(__VA_ARGS__))
#else
#define CHECK_NANS(...)
#endif

template <class T> struct vec2 {
	using Scalar = T;
	enum { vec_size = 2 };

	constexpr vec2(T x, T y) : x(x), y(y) { CHECK_NANS(x, y); }
	constexpr vec2() : x(0), y(0) {}
	constexpr vec2(CSpan<T, 2> v) : vec2(v[0], v[1]) {}
	constexpr vec2(NoInitTag) {}
	~vec2() { x.~T(), y.~T(); }

	vec2(const vec2 &) = default;
	vec2 &operator=(const vec2 &) = default;

	explicit vec2(T t) : x(t), y(t) {}
	template <class U, EnableIf<precise_conversion<U, T>>...>
	vec2(const vec2<U> &rhs) : vec2(T(rhs.x), T(rhs.y)) {}
	template <class U, EnableIf<!precise_conversion<U, T>>...>
	explicit vec2(const vec2<U> &rhs) : vec2(T(rhs.x), T(rhs.y)) {}

	vec2 operator*(const vec2 &rhs) const { return vec2(x * rhs.x, y * rhs.y); }
	vec2 operator/(const vec2 &rhs) const { return vec2(x / rhs.x, y / rhs.y); }
	vec2 operator+(const vec2 &rhs) const { return vec2(x + rhs.x, y + rhs.y); }
	vec2 operator-(const vec2 &rhs) const { return vec2(x - rhs.x, y - rhs.y); }
	vec2 operator*(T s) const { return vec2(x * s, y * s); }
	vec2 operator/(T s) const { return vec2(x / s, y / s); }
	vec2 operator-() const { return vec2(-x, -y); }

	T &operator[](int idx) { return v[idx]; }
	const T &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(vec2, x, y)

	Span<T, 2> values() { return v; }
	CSpan<T, 2> values() const { return v; }

	union {
		struct {
			T x, y;
		};
		T v[2];
	};
};

template <class T> struct vec3 {
	using Scalar = T;
	enum { vec_size = 3 };

	constexpr vec3(T x, T y, T z) : x(x), y(y), z(z) { CHECK_NANS(x, y, z); }
	constexpr vec3(const vec2<T> &xy, T z) : x(xy.x), y(xy.y), z(z) { CHECK_NANS(z); }
	constexpr vec3() : x(0), y(0), z(0) {}
	constexpr vec3(CSpan<T, 3> v) : vec3(v[0], v[1], v[2]) {}
	explicit vec3(T t) : x(t), y(t), z(t) { CHECK_NANS(t); }
	constexpr vec3(NoInitTag) {}
	~vec3() { x.~T(), y.~T(), z.~T(); }

	vec3(const vec3 &) = default;
	vec3 &operator=(const vec3 &) = default;

	template <class U, EnableIf<precise_conversion<U, T>>...>
	vec3(const vec3<U> &rhs) : vec3(T(rhs.x), T(rhs.y), T(rhs.z)) {}
	template <class U, EnableIf<!precise_conversion<U, T>>...>
	explicit vec3(const vec3<U> &rhs) : vec3(T(rhs.x), T(rhs.y), T(rhs.z)) {}

	vec3 operator*(const vec3 &rhs) const { return vec3(x * rhs.x, y * rhs.y, z * rhs.z); }
	vec3 operator/(const vec3 &rhs) const { return vec3(x / rhs.x, y / rhs.y, z / rhs.z); }
	vec3 operator+(const vec3 &rhs) const { return vec3(x + rhs.x, y + rhs.y, z + rhs.z); }
	vec3 operator-(const vec3 &rhs) const { return vec3(x - rhs.x, y - rhs.y, z - rhs.z); }
	vec3 operator*(T s) const { return vec3(x * s, y * s, z * s); }
	vec3 operator/(T s) const { return vec3(x / s, y / s, z / s); }
	vec3 operator-() const { return vec3(-x, -y, -z); }

	T &operator[](int idx) { return v[idx]; }
	const T &operator[](int idx) const { return v[idx]; }

	vec2<T> xy() const { return {x, y}; }
	vec2<T> xz() const { return {x, z}; }
	vec2<T> yz() const { return {y, z}; }

	FWK_ORDER_BY(vec3, x, y, z)

	Span<T, 3> values() { return v; }
	CSpan<T, 3> values() const { return v; }

	union {
		struct {
			T x, y, z;
		};
		T v[3];
	};
};

template <class T> struct vec4 {
	using Scalar = T;
	enum { vec_size = 4 };

	constexpr vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) { CHECK_NANS(x, y, z, w); }
	constexpr vec4(const vec2<T> &xy, T z, T w) : x(xy.x), y(xy.y), z(z), w(w) { CHECK_NANS(z, w); }
	constexpr vec4(const vec3<T> &xyz, T w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) { CHECK_NANS(w); }
	constexpr vec4() : x(0), y(0), z(0), w(0) {}
	constexpr vec4(CSpan<T, 4> v) : vec4(v[0], v[1], v[2], v[3]) {}
	explicit vec4(T t) : x(t), y(t), z(t), w(t) { CHECK_NANS(t); }
	constexpr vec4(NoInitTag) {}
	~vec4() { x.~T(), y.~T(), z.~T(), w.~T(); }

	vec4(const vec4 &) = default;
	vec4 &operator=(const vec4 &) = default;

	template <class U, EnableIf<precise_conversion<U, T>>...>
	vec4(const vec4<U> &rhs) : vec4(T(rhs.x), T(rhs.y), T(rhs.z), T(rhs.w)) {}
	template <class U, EnableIf<!precise_conversion<U, T>>...>
	explicit vec4(const vec4<U> &rhs) : vec4(T(rhs.x), T(rhs.y), T(rhs.z), T(rhs.w)) {}

	vec4 operator*(const vec4 &rhs) const {
		return vec4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
	}
	vec4 operator/(const vec4 &rhs) const {
		return vec4(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w);
	}
	vec4 operator+(const vec4 &rhs) const {
		return vec4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
	}
	vec4 operator-(const vec4 &rhs) const {
		return vec4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
	}
	vec4 operator*(T s) const { return vec4(x * s, y * s, z * s, w * s); }
	vec4 operator/(T s) const { return vec4(x / s, y / s, z / s, w / s); }
	vec4 operator-() const { return vec4(-x, -y, -z, -w); }

	T &operator[](int idx) { return v[idx]; }
	const T &operator[](int idx) const { return v[idx]; }

	vec2<T> xy() const { return {x, y}; }
	vec2<T> xz() const { return {x, z}; }
	vec2<T> yz() const { return {y, z}; }
	vec3<T> xyz() const { return {x, y, z}; }

	FWK_ORDER_BY(vec4, x, y, z, w)

	Span<T, 4> values() { return v; }
	CSpan<T, 4> values() const { return v; }

	union {
		struct {
			T x, y, z, w;
		};
		T v[4];
	};
};

template <class T, EnableIfVec<T>...> T operator*(typename T::Scalar s, const T &v) {
	return v * s;
}

#undef CHECK_NANS

// -------------------------------------------------------------------------------------------
// ---  Vector versions of basic math functions  ---------------------------------------------

template <class TVec, class TFunc, EnableIfVec<TVec, 2>...>
auto transform(const TVec &vec, const TFunc &func) {
	using TOut = MakeVec<decltype(func(vec[0])), 2>;
	return TOut{func(vec[0]), func(vec[1])};
}

template <class TVec, class TFunc, EnableIfVec<TVec, 3>...>
auto transform(const TVec &vec, const TFunc &func) {
	using TOut = MakeVec<decltype(func(vec[0])), 3>;
	return TOut{func(vec[0]), func(vec[1]), func(vec[2])};
}

template <class TVec, class TFunc, EnableIfVec<TVec, 4>...>
auto transform(const TVec &vec, const TFunc &func) {
	using TOut = MakeVec<decltype(func(vec[0])), 4>;
	return TOut{func(vec[0]), func(vec[1]), func(vec[2]), func(vec[3])};
}

template <class T, EnableIfVec<T, 2>...> T vmin(const T &lhs, const T &rhs) {
	return T(min(lhs[0], rhs[0]), min(lhs[1], rhs[1]));
}

template <class T, EnableIfVec<T, 3>...> T vmin(const T &lhs, const T &rhs) {
	return T(min(lhs[0], rhs[0]), min(lhs[1], rhs[1]), min(lhs[2], rhs[2]));
}

template <class T, EnableIfVec<T, 4>...> T vmin(const T &lhs, const T &rhs) {
	return T(min(lhs[0], rhs[0]), min(lhs[1], rhs[1]), min(lhs[2], rhs[2]), min(lhs[3], rhs[3]));
}

template <class T, EnableIfVec<T, 2>...> T vmax(const T &lhs, const T &rhs) {
	return T(max(lhs[0], rhs[0]), max(lhs[1], rhs[1]));
}

template <class T, EnableIfVec<T, 3>...> T vmax(const T &lhs, const T &rhs) {
	return T(max(lhs[0], rhs[0]), max(lhs[1], rhs[1]), max(lhs[2], rhs[2]));
}

template <class T, EnableIfVec<T, 4>...> T vmax(const T &lhs, const T &rhs) {
	return T(max(lhs[0], rhs[0]), max(lhs[1], rhs[1]), max(lhs[2], rhs[2]), max(lhs[3], rhs[3]));
}

template <class T, EnableIfVec<T>...> T vclamp(const T &vec, const T &tmin, const T &tmax) {
	return vmin(tmax, vmax(tmin, vec));
}

template <class T, EnableIfVec<T>...> auto vfloor(const T &v) {
	return transform(v, [](auto s) { return floor(s); });
}
template <class T, EnableIfVec<T>...> auto vceil(const T &v) {
	return transform(v, [](auto s) { return ceil(s); });
}
template <class T, EnableIfVec<T>...> auto vround(const T &v) {
	return transform(v, [](auto s) { return round(s); });
}
template <class T, EnableIfVec<T>...> T vabs(const T &v) {
	return transform(v, [](auto s) { return abs(s); });
}
template <class T, EnableIfFptVec<T>...> T vinv(const T &vec) {
	return transform(vec, [](auto v) { return inv(v); });
}

template <class TVec, class T, EnableIfIntegralVec<TVec>...>
auto vratioFloor(const TVec &v, T div) {
	return transform(v, [=](auto t) { return ratioFloor(t, div); });
}

template <class TVec, class T, EnableIfIntegralVec<TVec>...> auto vratioCeil(const TVec &v, T div) {
	return transform(v, [=](auto t) { return ratioCeil(t, div); });
}

// -------------------------------------------------------------------------------------------
// ---  Vector functions  --------------------------------------------------------------------

template <class T, EnableIfVec<T, 2>...> auto dot(const T &lhs, const T &rhs) {
	return lhs[0] * rhs[0] + lhs[1] * rhs[1];
}

template <class T, EnableIfVec<T, 3>...> auto dot(const T &lhs, const T &rhs) {
	return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

template <class T, EnableIfVec<T, 4>...> auto dot(const T &lhs, const T &rhs) {
	return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2] + lhs[3] * rhs[3];
}

template <class T, EnableIfFptVec<T>...> auto length(const T &vec) {
	return std::sqrt(dot(vec, vec));
}

template <class T, EnableIfVec<T>...> auto lengthSq(const T &vec) { return dot(vec, vec); }

template <class T, EnableIfFptVec<T>...> auto distance(const T &lhs, const T &rhs) {
	return length(lhs - rhs);
}

template <class T, EnableIfVec<T>...> auto distanceSq(const T &lhs, const T &rhs) {
	return lengthSq(lhs - rhs);
}

template <class T, EnableIfFptVec<T>...> T normalize(const T &v) { return v / length(v); }

template <class T, EnableIfVec<T, 2>...> auto asXZ(const T &v) {
	using TOut = MakeVec<typename T::Scalar, 3>;
	return TOut(v[0], 0, v[1]);
}
template <class T, EnableIfVec<T, 2>...> auto asXY(const T &v) {
	using TOut = MakeVec<typename T::Scalar, 3>;
	return TOut(v[0], v[1], 0);
}
template <class T, EnableIfVec<T, 2>...> auto asXZY(const T &xz, typename T::Scalar y) {
	using TOut = MakeVec<typename T::Scalar, 3>;
	return TOut(xz[0], y, xz[1]);
}

template <class T, EnableIfVec<T, 3>...> T asXZY(const T &v) { return {v[0], v[2], v[1]}; }

// Right-handed coordinate system
template <class T, EnableIfVec<T, 3>...> T cross(const T &a, const T &b) {
	return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

// Default orientation in all vector-related operations
// (cross products, rotations, etc.) is counter-clockwise.

template <class T, EnableIfVec<T, 2>...> auto cross(const T &a, const T &b) {
	return a[0] * b[1] - a[1] * b[0];
}

template <class T, EnableIfVec<T, 2>...> T perpendicular(const T &v) { return T(-v[1], v[0]); }

// TODO: we can't really check it properly for floating-point's...
template <class T, EnableIfFptVec<T>...> bool isNormalized(const T &vec) {
	return isAlmostOne(lengthSq(vec));
}

// Returns true if v2 is CCW to v1
template <class T> bool ccwSide(const vec2<T> &v1, const vec2<T> &v2) {
	using PV = vec2<Promote<T>>;
	return cross<PV>(v1, v2) > 0;
}

template <class T> bool cwSide(const vec2<T> &v1, const vec2<T> &v2) {
	using PV = vec2<Promote<T>>;
	return cross<PV>(v1, v2) < 0;
}

// Returns true if (point - from) is CCW to (to - from)
template <class T> bool ccwSide(const vec2<T> &from, const vec2<T> &to, const vec2<T> &point) {
	using PV = vec2<Promote<T>>;
	return dot<PV>(perpendicular(to - from), point - from) > 0;
}

template <class T> bool cwSide(const vec2<T> &from, const vec2<T> &to, const vec2<T> &point) {
	using PV = vec2<Promote<T>>;
	return dot<PV>(perpendicular(to - from), point - from) < 0;
}

template <class TVec, EnableIfVec<TVec>...> bool sameDirection(const TVec &vec1, const TVec &vec2) {
	using PT = PromoteIntegral<TVec>;
	using TCross = If<dim<TVec> == 2, Scalar<PT>, PT>;
	return cross<PT>(vec1, vec2) == TCross(0) && dot<PT>(vec1, vec2) > 0;
}

// -------------------------------------------------------------------------------------------
// ---  Additional declarations  -------------------------------------------------------------

struct DisabledInThisDimension;

template <class T, int N> using EnableInDimension = EnableIf<dim<T> == N, DisabledInThisDimension>;

DEFINE_ENUM(IsectClass, adjacent, point, segment, none);
using IsectFlags = EnumFlags<IsectClass>;
}

SERIALIZE_AS_POD(short2)
SERIALIZE_AS_POD(int2)
SERIALIZE_AS_POD(int3)
SERIALIZE_AS_POD(int4)

SERIALIZE_AS_POD(float2)
SERIALIZE_AS_POD(float3)
SERIALIZE_AS_POD(float4)
SERIALIZE_AS_POD(IRect)
SERIALIZE_AS_POD(FRect)
SERIALIZE_AS_POD(IBox)
SERIALIZE_AS_POD(FBox)
SERIALIZE_AS_POD(Matrix4)
SERIALIZE_AS_POD(Matrix3)
SERIALIZE_AS_POD(Quat)
SERIALIZE_AS_POD(Ray3F)

SERIALIZE_AS_POD(Segment2<float>)
SERIALIZE_AS_POD(Segment3<float>)
SERIALIZE_AS_POD(Segment2<double>)
SERIALIZE_AS_POD(Segment3<double>)
SERIALIZE_AS_POD(Segment2<int>)
