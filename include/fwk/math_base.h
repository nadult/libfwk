// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/light_tuple.h"
#include "fwk/sys_base.h"
#include "fwk_index_range.h"
#include <cmath>
#include <limits>

#ifndef __x86_64
#define FWK_USE_BOOST_MPC_INT
#endif

#ifdef FWK_USE_BOOST_MPC_INT
#include <boost/multiprecision/cpp_int.hpp>
#endif

namespace fwk {

// TODO: remove epsilons?
namespace dconstant {
	static constexpr const double pi = 3.14159265358979323846;
	static constexpr const double e = 2.7182818284590452354;
	static constexpr const double epsilon = 0.0001;
	static constexpr const double isect_epsilon = 0.000000001;
	static const double inf = std::numeric_limits<double>::infinity();
}

namespace fconstant {
	static constexpr const float pi = dconstant::pi;
	static constexpr const float e = dconstant::e;
	static constexpr const float epsilon = 0.0001f;
	static constexpr const float isect_epsilon = 0.000000001f;
	static const float inf = std::numeric_limits<float>::infinity();
}

template <class Real> struct constant {
	static_assert(std::is_floating_point<Real>::value, "");

	// TODO: long doubles support ?
	static const constexpr Real pi = dconstant::pi;
	static const constexpr Real e = dconstant::e;
	static const constexpr Real epsilon = dconstant::epsilon;
	static const constexpr Real isect_epsilon = dconstant::isect_epsilon;
	static Real inf() { return std::numeric_limits<Real>::infinity(); }
};

template <class T> struct vec2;
template <class T> struct vec3;
template <class T> struct vec4;

template <class T> struct Rational;
template <class T> struct Rational2;
template <class T> struct Rational3;

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

pair<float, float> sincos(float radians);
pair<double, double> sincos(double radians);

inline bool isnan(float v) { return std::isnan(v); }
inline bool isnan(double v) { return std::isnan(v); }

inline int abs(int s) { return std::abs(s); }
inline double abs(double s) { return std::abs(s); }
inline float abs(float s) { return std::abs(s); }

inline double inv(double s) { return 1.0 / s; }
inline float inv(float s) { return 1.0f / s; }

using std::ceil;
using std::floor;

struct NotAReal;
struct NotAIntegral;
struct NotAScalar;
struct NotAMathObject;
template <int N> struct NotAValidVec;

namespace detail {

	template <class T> struct IsIntegral : public std::is_integral<T> {};
	template <class T> struct IsRational : public std::false_type {};
	template <class T> struct IsReal : public std::is_floating_point<T> {};
	template <> struct IsIntegral<qint> : public std::true_type {};
	template <class T> struct IsRational<Rational<T>> : public std::true_type {};

	template <class T> struct IsScalar {
		static constexpr bool value =
			IsReal<T>::value || IsIntegral<T>::value || IsRational<T>::value;
	};

	template <class T, template <class> class Trait> struct ScalarTrait {
		template <class U>
		static typename std::enable_if<Trait<typename U::Scalar>::value, char>::type test(U *);
		template <class U> static long test(...);
		static constexpr bool value = sizeof(test<T>(nullptr)) == 1;
	};

	template <class T, int N> struct MakeVec { using type = NotAValidVec<N>; };
	template <class T> struct MakeVec<T, 2> { using type = vec2<T>; };
	template <class T> struct MakeVec<T, 3> { using type = vec3<T>; };
	template <class T> struct MakeVec<T, 4> { using type = vec4<T>; };

	template <class T> struct VecScalar {
		template <class U> static auto test(U *) -> typename U::Scalar;
		template <class U> static NotAValidVec<0> test(...);
		using type = decltype(test<T>(nullptr));
	};

	template <class T> struct VecSize {
		template <int N> struct Size { static constexpr int value = N; };
		template <class U> static auto test(U *) -> Size<U::vec_size>;
		template <class U> static Size<0> test(...);
		static constexpr int value = decltype(test<T>(nullptr))::value;
	};

	template <class T> struct Promotion { using type = T; };

	template <class From, class To> struct PreciseConversion {
		static constexpr bool resolve() {
			if constexpr(is_same<From, To>)
				return true;
			using PFrom = typename Promotion<From>::type;
			if constexpr(!is_same<PFrom, From>)
				return PreciseConversion<PFrom, To>::value;
			return false;
		}
		static constexpr bool value = resolve();
	};

#define PROMOTION(base, promoted)                                                                  \
	template <> struct Promotion<base> { using type = promoted; };
#define PRECISE(from, to)                                                                          \
	template <> struct PreciseConversion<from, to> { static constexpr bool value = true; };

	PROMOTION(short, int);
	PROMOTION(int, llint);
	PROMOTION(llint, qint);
	PROMOTION(float, double);

	PRECISE(short, float)
	PRECISE(int, double)
	PRECISE(float, double)

	// TODO: long doubles support?

#undef PROMOTION
#undef PRECISE

	template <class T> auto promote() {
		if constexpr(VecSize<T>::value > 0) {
			using Promoted = typename Promotion<typename T::Scalar>::type;
			return typename MakeVec<Promoted, T::vec_size>::type();
		} else {
			return typename detail::Promotion<T>::type();
		}
	}
}

template <class T> static constexpr bool is_real = detail::IsReal<T>::value;
template <class T> static constexpr bool is_integral = detail::IsIntegral<T>::value;
template <class T> static constexpr bool is_rational = detail::IsRational<T>::value;
template <class T> static constexpr bool is_scalar = detail::IsScalar<T>::value;

template <class T>
static constexpr bool is_real_object = detail::ScalarTrait<T, detail::IsReal>::value;

template <class T>
static constexpr bool is_integral_object = detail::ScalarTrait<T, detail::IsIntegral>::value;

template <class T>
static constexpr bool is_rational_object = detail::ScalarTrait<T, detail::IsRational>::value;

template <class T>
static constexpr bool is_math_object = detail::ScalarTrait<T, detail::IsScalar>::value;

template <class T> constexpr int vec_size = detail::VecSize<T>::value;
template <class T> using VecScalar = typename detail::VecScalar<T>::type;

template <class T, int N = 0>
static constexpr bool is_vec = (vec_size<T> == N || N == 0) && vec_size<T>> 0;

template <class T, int N = 0>
static constexpr bool is_real_vec = is_vec<T, N> &&is_real<VecScalar<T>>;

template <class T, int N = 0>
static constexpr bool is_rational_or_real_vec = is_vec<T, N> && (is_real<VecScalar<T>> ||
																 is_rational<VecScalar<T>>);

template <class T, int N = 0>
static constexpr bool is_integral_vec = is_vec<T, N> &&is_integral<VecScalar<T>>;

template <class T, int N = 0>
static constexpr bool is_rational_vec = is_vec<T, N> &&is_rational<VecScalar<T>>;

template <class T> using EnableIfScalar = EnableIf<is_scalar<T>, NotAScalar>;
template <class T> using EnableIfReal = EnableIf<is_real<T>, NotAReal>;
template <class T> using EnableIfIntegral = EnableIf<is_integral<T>, NotAIntegral>;

template <class T> using EnableIfMathObject = EnableIf<is_math_object<T>, NotAMathObject>;

template <class T, int N = 0> using EnableIfVec = EnableIf<is_vec<T, N>, NotAValidVec<N>>;
template <class T, int N = 0> using EnableIfRealVec = EnableIf<is_real_vec<T, N>, NotAValidVec<N>>;
template <class T, int N = 0>
using EnableIfRationalOrRealVec = EnableIf<is_rational_or_real_vec<T, N>, NotAValidVec<N>>;
template <class T, int N = 0>
using EnableIfIntegralVec = EnableIf<is_integral_vec<T, N>, NotAValidVec<N>>;

template <class T, int N> using MakeVec = typename detail::MakeVec<T, N>::type;

template <class From, class To> constexpr bool preciseConversion() {
	if constexpr(is_vec<From> && is_vec<To>)
		return int(From::vec_size) == int(To::vec_size) &&
			   detail::PreciseConversion<typename From::Scalar, typename To::Scalar>::value;
	return detail::PreciseConversion<From, To>::value;
};

template <class T> using Promote = decltype(detail::promote<T>());
template <class T>
using PromoteIntegral = Conditional<is_integral<T> || is_integral_vec<T>, Promote<T>, T>;

template <class T> struct ToReal { using type = double; };
template <> struct ToReal<float> { using type = float; };
template <> struct ToReal<short> { using type = float; };

template <class T, class... Args> bool isnan(T val0, Args... vals) {
	return isnan(val0) || isnan(vals...);
}

template <class T, EnableIfRealVec<T>...> bool isnan(const T &v) {
	return anyOf(v.values(), [](auto s) { return isnan(s); });
}

template <class TRange, class T = RangeBase<TRange>, EnableIf<is_real<T> || is_real_vec<T>>...>
bool isnan(const TRange &range) {
	return anyOf(range, [](const auto &elem) { return isnan(elem); });
}

template <class T, EnableIfIntegral<T>...> T nextPow2(T val) {
	T out = 1;
	while(out < val)
		out *= 2;
	return out;
}

#ifdef FWK_CHECK_NANS
#define CHECK_NANS(...)                                                                            \
	if constexpr(is_real<T>)                                                                       \
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

	vec2(const vec2 &) = default;
	vec2 &operator=(const vec2 &) = default;

	explicit vec2(T t) : x(t), y(t) {}
	template <class U, EnableIf<preciseConversion<U, T>()>...>
	vec2(const vec2<U> &rhs) : vec2(T(rhs.x), T(rhs.y)) {}
	template <class U, EnableIf<!preciseConversion<U, T>()>...>
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

	vec3(const vec3 &) = default;
	vec3 &operator=(const vec3 &) = default;

	template <class U, EnableIf<preciseConversion<U, T>()>...>
	vec3(const vec3<U> &rhs) : vec3(T(rhs.x), T(rhs.y), T(rhs.z)) {}
	template <class U, EnableIf<!preciseConversion<U, T>()>...>
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

	vec4(const vec4 &) = default;
	vec4 &operator=(const vec4 &) = default;

	template <class U, EnableIf<preciseConversion<U, T>()>...>
	vec4(const vec4<U> &rhs) : vec4(T(rhs.x), T(rhs.y), T(rhs.z), T(rhs.w)) {}
	template <class U, EnableIf<!preciseConversion<U, T>()>...>
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

	Span<T, 3> values() { return v; }
	CSpan<T, 3> values() const { return v; }

	union {
		struct {
			T x, y, z, w;
		};
		T v[4];
	};
};

#undef CHECK_NANS

template <class T, class T1, EnableIfMathObject<T>...> void operator+=(T &a, const T1 &b) {
	a = a + b;
}
template <class T, class T1, EnableIfMathObject<T>...> void operator-=(T &a, const T1 &b) {
	a = a - b;
}
template <class T, class T1, EnableIfMathObject<T>...> void operator*=(T &a, const T1 &b) {
	a = a * b;
}
template <class T, class T1, EnableIfMathObject<T>...> void operator/=(T &a, const T1 &b) {
	a = a / b;
}

template <class T, EnableIfMathObject<T>...> bool operator>(const T &a, const T &b) {
	return b < a;
}

template <class T, EnableIfMathObject<T>...> bool operator>=(const T &a, const T &b) {
	return !(a < b);
}

template <class T, EnableIfMathObject<T>...> bool operator<=(const T &a, const T &b) {
	return !(b < a);
}

template <class T, EnableIfScalar<T>...> T clamp(const T &obj, const T &tmin, const T &tmax) {
	return min(tmax, max(tmin, obj));
}

template <class Real, EnableIfReal<Real>...> Real degToRad(Real v) {
	return v * (Real(2.0) * constant<Real>::pi / Real(360.0));
}

template <class Real, EnableIfReal<Real>...> Real radToDeg(Real v) {
	return v * (Real(360.0) / (Real(2.0) * constant<Real>::pi));
}

// Return angle in range (0; 2 * PI)
float normalizeAngle(float radians);

template <class Obj, class Scalar> inline Obj lerp(const Obj &a, const Obj &b, const Scalar &x) {
	return (b - a) * x + a;
}

template <class T, EnableIfVec<T>...> T operator*(typename T::Scalar s, const T &v) {
	return v * s;
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

template <class T, EnableIfScalar<T>...> T abs(T value) { return value < T(0) ? -value : value; }

// Nonstandard behaviour: rounding half up (0.5 -> 1, -0.5 -> 0)
template <class T, EnableIfReal<T>...> T round(T value) { return floor(value + T(0.5)); }

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

template <class T, EnableIfIntegral<T>...> T ratioFloor(T value, T div) {
	return value < T(0) ? (value - div + T(1)) / div : value / div;
}

template <class T, EnableIfIntegral<T>...> T ratioCeil(T value, T div) {
	return value > T(0) ? (value + div - T(1)) / div : value / div;
}

template <class TVec, class T, EnableIfIntegralVec<TVec>...>
auto ratioFloor(const TVec &vec, T div) {
	return fwk::transform(vec, [=](const auto &t) { return ratioFloor(t, div); });
}

template <class TVec, class T, EnableIfIntegralVec<TVec>...>
auto ratioCeil(const TVec &vec, T div) {
	return fwk::transform(vec, [=](const auto &t) { return ratioCeil(t, div); });
}
template <class T, EnableIfRationalOrRealVec<T>...> auto vfloor(const T &vec) {
	return transform(vec, [](const auto &t) { return floor(t); });
}

template <class T, EnableIfRationalOrRealVec<T>...> auto vceil(T vec) {
	return transform(vec, [](const auto &t) { return ceil(t); });
}

template <class T, EnableIfRationalOrRealVec<T>...> auto vround(T vec) {
	return transform(vec, [](const auto &t) { return round(t); });
}

template <class T, EnableIfVec<T>...> T vabs(T vec) {
	return transform(vec, [](const auto &t) { return abs(t); });
}

template <class T, EnableIfVec<T, 2>...> auto dot(const T &lhs, const T &rhs) {
	return lhs[0] * rhs[0] + lhs[1] * rhs[1];
}

template <class T, EnableIfVec<T, 3>...> auto dot(const T &lhs, const T &rhs) {
	return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

template <class T, EnableIfVec<T, 4>...> auto dot(const T &lhs, const T &rhs) {
	return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2] + lhs[3] * rhs[3];
}

template <class T, EnableIfRealVec<T>...> auto length(const T &vec) {
	return std::sqrt(dot(vec, vec));
}

template <class T, EnableIfVec<T>...> auto lengthSq(const T &vec) { return dot(vec, vec); }

template <class T, EnableIfRealVec<T>...> auto distance(const T &lhs, const T &rhs) {
	return length(lhs - rhs);
}

template <class T, EnableIfVec<T>...> auto distanceSq(const T &lhs, const T &rhs) {
	return lengthSq(lhs - rhs);
}

template <class T, EnableIfRealVec<T>...> T normalize(const T &v) { return v / length(v); }

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

template <class T, EnableIfRealVec<T>...> T vinv(const T &vec) {
	return transform(vec, [](const auto &v) { return inv(v); });
}

// Right-handed coordinate system
template <class T, EnableIfVec<T, 3>...> T cross(const T &a, const T &b) {
	return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

// Default orientation in all vector-related operations
// (cross products, rotations, etc.) is counter-clockwise.

template <class T, EnableIfVec<T, 2>...> auto cross(const T &a, const T &b) {
	return a[0] * b[1] - a[1] * b[0];
}

template <class T, EnableIfVec<T, 2>...> T perpendicular(const T &v) { return {-v[1], v[0]}; }

float vectorToAngle(const float2 &normalized_vector);
double vectorToAngle(const double2 &normalized_vector);

float2 angleToVector(float radians);
double2 angleToVector(double radians);

float2 rotateVector(const float2 &vec, float radians);
double2 rotateVector(const double2 &vec, double radians);
float3 rotateVector(const float3 &pos, const float3 &axis, float angle);
double3 rotateVector(const double3 &pos, const double3 &axis, double angle);

// Returns CCW angle from vec1 to vec2 in range <0; 2*PI)
float angleBetween(const float2 &vec1, const float2 &vec2);
double angleBetween(const double2 &vec1, const double2 &vec2);

// Returns positive value if vector turned left, negative if it turned right;
// Returned angle is in range <-PI, PI>
float angleTowards(const float2 &prev, const float2 &cur, const float2 &next);
double angleTowards(const double2 &prev, const double2 &cur, const double2 &next);

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

// TODO: remove it?
template <class T, EnableIfRealVec<T>...>
bool areClose(const T &a, const T &b,
			  typename T::Scalar epsilon_sq = constant<typename T::Scalar>::epsilon) {
	return distanceSq(a, b) < epsilon_sq;
}

// TODO: we can't really check it properly for floating-point's...
template <class T, EnableIfRealVec<T>...> bool isNormalized(const T &vec) {
	using Real = typename T::Scalar;
	auto length_sq = lengthSq(vec);
	return length_sq >= Real(1) - constant<Real>::epsilon &&
		   length_sq <= Real(1) + constant<Real>::epsilon;
}

template <class T, EnableIfVec<T>...> bool isZero(const T &vec) { return vec == T(); }

float frand();
float angleDistance(float a, float b);
float blendAngles(float initial, float target, float step);

// Returns angle in range <0, 2 * PI)
float normalizeAngle(float angle);

template <class T, EnableIfIntegral<T>...> constexpr bool isPowerOfTwo(T value) {
	return (value & (value - 1)) == 0;
}

constexpr int countLeadingZeros(uint value) { return value ? __builtin_clz(value) : 32; }
constexpr int countLeadingZeros(u64 value) { return value ? __builtin_clzll(value) : 64; }
constexpr int countTrailingZeros(uint value) { return value ? __builtin_clz(value) : 32; }
constexpr int countTrailingZeros(u64 value) { return value ? __builtin_clzll(value) : 64; }
constexpr int countBits(uint value) { return __builtin_popcount(value); }
constexpr int countBits(u64 value) { return __builtin_popcountll(value); }

class Matrix3;
class Matrix4;

template <class T> class Box;
template <class T> struct Interval;
template <class T> class IsectParam;

template <class T, int N> class Triangle;
template <class T, int N> class Plane;
template <class T, int N> class Ray;
template <class T, int N> class Segment;
template <class T, int N> class ISegment;

template <class T> using ISegment2 = ISegment<T, 2>;
template <class T> using ISegment3 = ISegment<T, 3>;
template <class T> using Segment2 = Segment<T, 2>;
template <class T> using Segment3 = Segment<T, 3>;

template <class T> using Triangle2 = Triangle<T, 2>;
template <class T> using Triangle3 = Triangle<T, 3>;

template <class T> using Plane2 = Plane<T, 2>;
template <class T> using Plane3 = Plane<T, 3>;

template <class T> using Ray2 = Ray<T, 2>;
template <class T> using Ray3 = Ray<T, 3>;

template <class T> using Box2 = Box<MakeVec<T, 2>>;
template <class T> using Box3 = Box<MakeVec<T, 3>>;

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

using Segment3I = ISegment<int, 3>;
using Segment2I = ISegment<int, 2>;

using IRect = Box<int2>;
using FRect = Box<float2>;
using DRect = Box<double2>;

using IBox = Box<int3>;
using FBox = Box<float3>;
using DBox = Box<double3>;

class Cylinder;
class Frustum;
class Matrix3;
class Matrix4;
class Quat;
class AxisAngle;
class AffineTrans;
class Projection;
class Tetrahedron;
class Random;

struct DisabledInThisDimension;

template <class T, int N> using EnableInDimension = EnableIf<is_vec<T, N>, DisabledInThisDimension>;

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
SERIALIZE_AS_POD(ISegment2<int>)
