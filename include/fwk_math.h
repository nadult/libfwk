// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys_base.h"
#include "fwk_index_range.h"
#include <cmath>
#include <limits>

namespace fwk {

// Note: naming convention:
// in math module we have vecs (vector2, vector3, vector4) not vectors
// to differentiate from fwk::Vector

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

pair<float, float> sincos(float radians);
pair<double, double> sincos(double radians);

bool isnan(float);
bool isnan(double);

template <class T> struct vector2;
template <class T> struct vector3;
template <class T> struct vector4;

using llint = long long;
using qint = __int128_t;

using short2 = vector2<short>;
using short3 = vector3<short>;
using short4 = vector4<short>;
using int2 = vector2<int>;
using int3 = vector3<int>;
using int4 = vector4<int>;
using float2 = vector2<float>;
using float3 = vector3<float>;
using float4 = vector4<float>;
using double2 = vector2<double>;
using double3 = vector3<double>;
using double4 = vector4<double>;
using llint2 = vector2<llint>;
using llint3 = vector3<llint>;
using llint4 = vector4<llint>;
using qint2 = vector2<qint>;
using qint3 = vector3<qint>;
using qint4 = vector4<qint>;

pair<float, float> sincos(float radians);
pair<double, double> sincos(double radians);

bool isnan(float);
bool isnan(double);

template <class... T> bool isnan(T... values) { return (... || isnan(values)); }

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
template <int N> struct NotAValidVector;

struct NotAReal;
struct NotAIntegral;
struct NotAScalar;
struct NotAMathObject;
template <int N> struct NotAValidVector;

namespace detail {

	template <class T> struct IsIntegral : public std::is_integral<T> {};
	template <class T> struct IsRational : public std::false_type {};
	template <class T> struct IsReal : public std::is_floating_point<T> {};
	template <> struct IsIntegral<qint> : public std::true_type {};

	template <class T> struct IsScalar {
		enum { value = IsReal<T>::value || IsIntegral<T>::value || IsRational<T>::value };
	};

	template <class T, template <class> class Trait> struct ScalarTrait {
		template <class U>
		static typename std::enable_if<Trait<typename U::Scalar>::value, char>::type test(U *);
		template <class U> static long test(...);
		enum { value = sizeof(test<T>(nullptr)) == 1 };
	};

	template <class T, int N> struct MakeVector { using type = NotAValidVector<N>; };
	template <class T> struct MakeVector<T, 2> { using type = vector2<T>; };
	template <class T> struct MakeVector<T, 3> { using type = vector3<T>; };
	template <class T> struct MakeVector<T, 4> { using type = vector4<T>; };

	template <class T> struct VecScalar {
		template <class U> static auto test(U *) -> typename U::Scalar;
		template <class U> static NotAValidVector<0> test(...);
		using type = decltype(test<T>(nullptr));
	};

	template <class T> struct VecSize {
		template <int N> struct Size {
			enum { value = N };
		};
		template <class U> static auto test(U *) -> Size<U::vector_size>;
		template <class U> static Size<0> test(...);
		enum { value = decltype(test<T>(nullptr))::value };
	};

	template <class T, class Enable = void> struct Promotion { using type = T; };

	template <class From, class To> struct PreciseConversion {
		static constexpr bool resolve() {
			if constexpr(isSame<From, To>())
				return true;
			using PFrom = typename Promotion<From>::type;
			if constexpr(!isSame<PFrom, From>())
				return PreciseConversion<PFrom, To>::value;
			return false;
		}
		enum { value = resolve() };
	};

#define PROMOTION(base, promoted)                                                                  \
	template <> struct Promotion<base> { using type = promoted; };
#define PRECISE(from, to)                                                                          \
	template <> struct PreciseConversion<from, to> {                                               \
		enum { value = true };                                                                     \
	};

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
			return typename MakeVector<Promoted, T::vector_size>::type();
		} else {
			return typename detail::Promotion<T>::type();
		}
	}
}

template <class T> constexpr bool isReal() { return detail::IsReal<T>::value; }
template <class T> constexpr bool isIntegral() { return detail::IsIntegral<T>::value; }
template <class T> constexpr bool isRational() { return detail::IsRational<T>::value; }
template <class T> constexpr bool isScalar() { return detail::IsScalar<T>::value; }

template <class T> constexpr bool isRealObject() {
	return detail::ScalarTrait<T, detail::IsReal>::value;
}
template <class T> constexpr bool isIntegralObject() {
	return detail::ScalarTrait<T, detail::IsIntegral>::value;
}
template <class T> constexpr bool isRationalObject() {
	return detail::ScalarTrait<T, detail::IsRational>::value;
}
template <class T> constexpr bool isMathObject() {
	return detail::ScalarTrait<T, detail::IsScalar>::value;
}

template <class T> constexpr int vec_size = detail::VecSize<T>::value;
template <class T> using VecScalar = typename detail::VecScalar<T>::type;

template <class T, int N = 0> constexpr bool isVector() {
	return (vec_size<T> == N || N == 0) && vec_size<T>> 0;
}
template <class T, int N = 0> constexpr bool isRealVector() {
	return isVector<T, N>() && isReal<VecScalar<T>>();
}
template <class T, int N = 0> constexpr bool isRationalOrRealVector() {
	return isVector<T, N>() && (isReal<VecScalar<T>>() || isRational<VecScalar<T>>());
}
template <class T, int N = 0> constexpr bool isIntegralVector() {
	return isVector<T, N>() && isIntegral<VecScalar<T>>();
}
template <class T, int N = 0> constexpr bool isRationalVector() {
	return isVector<T, N>() && isRational<VecScalar<T>>();
}

template <class T> using EnableIfScalar = EnableIf<isScalar<T>(), NotAScalar>;
template <class T> using EnableIfReal = EnableIf<isReal<T>(), NotAReal>;
template <class T> using EnableIfIntegral = EnableIf<isIntegral<T>(), NotAIntegral>;

template <class T> using EnableIfMathObject = EnableIf<isMathObject<T>(), NotAMathObject>;

template <class T, int N = 0> using EnableIfVector = EnableIf<isVector<T, N>(), NotAValidVector<N>>;
template <class T, int N = 0>
using EnableIfRealVector = EnableIf<isRealVector<T, N>(), NotAValidVector<N>>;
template <class T, int N = 0>
using EnableIfRationalOrRealVector = EnableIf<isRationalOrRealVector<T, N>(), NotAValidVector<N>>;
template <class T, int N = 0>
using EnableIfIntegralVector = EnableIf<isIntegralVector<T, N>(), NotAValidVector<N>>;

template <class T, int N> using MakeVector = typename detail::MakeVector<T, N>::type;

template <class From, class To> constexpr bool preciseConversion() {
	if constexpr(isVector<From>() && isVector<To>())
		return From::vector_size == To::vector_size &&
			   detail::PreciseConversion<typename From::Scalar, typename To::Scalar>::value;
	return detail::PreciseConversion<From, To>::value;
};

template <class T> using Promote = decltype(detail::promote<T>());
template <class T>
using PromoteIntegral = Conditional<isIntegral<T>() || isIntegralVector<T>(), Promote<T>, T>;

template <class T> struct ToReal { using type = double; };
template <> struct ToReal<float> { using type = float; };
template <> struct ToReal<short> { using type = float; };

template <class T> struct vector2 {
	using Scalar = T;
	enum { vector_size = 2 };

	constexpr vector2(T x, T y) : x(x), y(y) {}
	constexpr vector2() : x(0), y(0) {}
	constexpr vector2(CSpan<T, 2> v) : vector2(v[0], v[1]) {}

	explicit vector2(T t) : x(t), y(t) {}
	template <class U, EnableIf<preciseConversion<U, T>()>...>
	vector2(const vector2<U> &rhs) : vector2(T(rhs.x), T(rhs.y)) {}
	template <class U, EnableIf<!preciseConversion<U, T>()>...>
	explicit vector2(const vector2<U> &rhs) : vector2(T(rhs.x), T(rhs.y)) {}

	vector2 operator*(const vector2 &rhs) const { return vector2(x * rhs.x, y * rhs.y); }
	vector2 operator/(const vector2 &rhs) const { return vector2(x / rhs.x, y / rhs.y); }
	vector2 operator+(const vector2 &rhs) const { return vector2(x + rhs.x, y + rhs.y); }
	vector2 operator-(const vector2 &rhs) const { return vector2(x - rhs.x, y - rhs.y); }
	vector2 operator*(T s) const { return vector2(x * s, y * s); }
	vector2 operator/(T s) const { return vector2(x / s, y / s); }
	vector2 operator-() const { return vector2(-x, -y); }

	T &operator[](int idx) { return v[idx]; }
	const T &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(vector2, x, y)

	Span<T, 2> values() { return v; }
	CSpan<T, 2> values() const { return v; }

	union {
		struct {
			T x, y;
		};
		T v[2];
	};
};

template <class T> struct vector3 {
	using Scalar = T;
	enum { vector_size = 3 };

	constexpr vector3(T x, T y, T z) : x(x), y(y), z(z) {}
	constexpr vector3(const vector2<T> &xy, T z) : x(xy.x), y(xy.y), z(z) {}
	constexpr vector3() : x(0), y(0), z(0) {}
	constexpr vector3(CSpan<T, 3> v) : vector3(v[0], v[1], v[2]) {}
	explicit vector3(T t) : x(t), y(t), z(t) {}

	template <class U, EnableIf<preciseConversion<U, T>()>...>
	vector3(const vector3<U> &rhs) : vector3(T(rhs.x), T(rhs.y), T(rhs.z)) {}
	template <class U, EnableIf<!preciseConversion<U, T>()>...>
	explicit vector3(const vector3<U> &rhs) : vector3(T(rhs.x), T(rhs.y), T(rhs.z)) {}

	vector3 operator*(const vector3 &rhs) const { return vector3(x * rhs.x, y * rhs.y, z * rhs.z); }
	vector3 operator/(const vector3 &rhs) const { return vector3(x / rhs.x, y / rhs.y, z / rhs.z); }
	vector3 operator+(const vector3 &rhs) const { return vector3(x + rhs.x, y + rhs.y, z + rhs.z); }
	vector3 operator-(const vector3 &rhs) const { return vector3(x - rhs.x, y - rhs.y, z - rhs.z); }
	vector3 operator*(T s) const { return vector3(x * s, y * s, z * s); }
	vector3 operator/(T s) const { return vector3(x / s, y / s, z / s); }
	vector3 operator-() const { return vector3(-x, -y, -z); }

	T &operator[](int idx) { return v[idx]; }
	const T &operator[](int idx) const { return v[idx]; }

	vector2<T> xy() const { return {x, y}; }
	vector2<T> xz() const { return {x, z}; }
	vector2<T> yz() const { return {y, z}; }

	FWK_ORDER_BY(vector3, x, y, z)

	Span<T, 3> values() { return v; }
	CSpan<T, 3> values() const { return v; }

	union {
		struct {
			T x, y, z;
		};
		T v[3];
	};
};

template <class T> struct vector4 {
	using Scalar = T;
	enum { vector_size = 4 };

	constexpr vector4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
	constexpr vector4(const vector2<T> &xy, T z, T w) : x(xy.x), y(xy.y), z(z), w(w) {}
	constexpr vector4(const vector3<T> &xyz, T w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
	constexpr vector4() : x(0), y(0), z(0), w(0) {}
	constexpr vector4(CSpan<T, 4> v) : vector4(v[0], v[1], v[2], v[3]) {}
	explicit vector4(T t) : x(t), y(t), z(t), w(t) {}

	template <class U, EnableIf<preciseConversion<U, T>()>...>
	vector4(const vector4<U> &rhs) : vector4(T(rhs.x), T(rhs.y), T(rhs.z), T(rhs.w)) {}
	template <class U, EnableIf<!preciseConversion<U, T>()>...>
	explicit vector4(const vector4<U> &rhs) : vector4(T(rhs.x), T(rhs.y), T(rhs.z), T(rhs.w)) {}

	vector4 operator*(const vector4 &rhs) const {
		return vector4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
	}
	vector4 operator/(const vector4 &rhs) const {
		return vector4(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w);
	}
	vector4 operator+(const vector4 &rhs) const {
		return vector4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
	}
	vector4 operator-(const vector4 &rhs) const {
		return vector4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
	}
	vector4 operator*(T s) const { return vector4(x * s, y * s, z * s, w * s); }
	vector4 operator/(T s) const { return vector4(x / s, y / s, z / s, w / s); }
	vector4 operator-() const { return vector4(-x, -y, -z, -w); }

	T &operator[](int idx) { return v[idx]; }
	const T &operator[](int idx) const { return v[idx]; }

	vector2<T> xy() const { return {x, y}; }
	vector2<T> xz() const { return {x, z}; }
	vector2<T> yz() const { return {y, z}; }
	vector3<T> xyz() const { return {x, y, z}; }

	FWK_ORDER_BY(vector4, x, y, z, w)

	Span<T, 3> values() { return v; }
	CSpan<T, 3> values() const { return v; }

	union {
		struct {
			T x, y, z, w;
		};
		T v[4];
	};
};

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

template <class T, EnableIfVector<T>...> T operator*(typename T::Scalar s, const T &v) {
	return v * s;
}

template <class T, EnableIfVector<T, 2>...> T vmin(const T &lhs, const T &rhs) {
	return T(min(lhs[0], rhs[0]), min(lhs[1], rhs[1]));
}

template <class T, EnableIfVector<T, 3>...> T vmin(const T &lhs, const T &rhs) {
	return T(min(lhs[0], rhs[0]), min(lhs[1], rhs[1]), min(lhs[2], rhs[2]));
}

template <class T, EnableIfVector<T, 4>...> T vmin(const T &lhs, const T &rhs) {
	return T(min(lhs[0], rhs[0]), min(lhs[1], rhs[1]), min(lhs[2], rhs[2]), min(lhs[3], rhs[3]));
}

template <class T, EnableIfVector<T, 2>...> T vmax(const T &lhs, const T &rhs) {
	return T(max(lhs[0], rhs[0]), max(lhs[1], rhs[1]));
}

template <class T, EnableIfVector<T, 3>...> T vmax(const T &lhs, const T &rhs) {
	return T(max(lhs[0], rhs[0]), max(lhs[1], rhs[1]), max(lhs[2], rhs[2]));
}

template <class T, EnableIfVector<T, 4>...> T vmax(const T &lhs, const T &rhs) {
	return T(max(lhs[0], rhs[0]), max(lhs[1], rhs[1]), max(lhs[2], rhs[2]), max(lhs[3], rhs[3]));
}

template <class T, EnableIfVector<T>...> T vclamp(const T &vec, const T &tmin, const T &tmax) {
	return vmin(tmax, vmax(tmin, vec));
}

template <class T, EnableIfScalar<T>...> T abs(T value) { return value < T(0) ? -value : value; }

// Nonstandard behaviour: rounding half up (0.5 -> 1, -0.5 -> 0)
template <class T, EnableIfReal<T>...> T round(T value) { return floor(value + T(0.5)); }

template <class TVec, class TFunc, EnableIfVector<TVec, 2>...>
auto transform(const TVec &vec, const TFunc &func) {
	using TOut = MakeVector<decltype(func(vec[0])), 2>;
	return TOut{func(vec[0]), func(vec[1])};
}

template <class TVec, class TFunc, EnableIfVector<TVec, 3>...>
auto transform(const TVec &vec, const TFunc &func) {
	using TOut = MakeVector<decltype(func(vec[0])), 3>;
	return TOut{func(vec[0]), func(vec[1]), func(vec[2])};
}

template <class TVec, class TFunc, EnableIfVector<TVec, 4>...>
auto transform(const TVec &vec, const TFunc &func) {
	using TOut = MakeVector<decltype(func(vec[0])), 4>;
	return TOut{func(vec[0]), func(vec[1]), func(vec[2]), func(vec[3])};
}

template <class T, EnableIfRationalOrRealVector<T>...> auto vfloor(const T &vec) {
	return transform(vec, [](const auto &t) { return floor(t); });
}

template <class T, EnableIfRationalOrRealVector<T>...> auto vceil(T vec) {
	return transform(vec, [](const auto &t) { return ceil(t); });
}

template <class T, EnableIfRationalOrRealVector<T>...> auto vround(T vec) {
	return transform(vec, [](const auto &t) { return round(t); });
}

template <class T, EnableIfVector<T>...> T vabs(T vec) {
	return transform(vec, [](const auto &t) { return abs(t); });
}

template <class T, EnableIfVector<T, 2>...> auto dot(const T &lhs, const T &rhs) {
	return lhs[0] * rhs[0] + lhs[1] * rhs[1];
}

template <class T, EnableIfVector<T, 3>...> auto dot(const T &lhs, const T &rhs) {
	return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

template <class T, EnableIfVector<T, 4>...> auto dot(const T &lhs, const T &rhs) {
	return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2] + lhs[3] * rhs[3];
}

template <class T, EnableIfRealVector<T>...> auto length(const T &vec) {
	return std::sqrt(dot(vec, vec));
}

template <class T, EnableIfVector<T>...> auto lengthSq(const T &vec) { return dot(vec, vec); }

template <class T, EnableIfRealVector<T>...> auto distance(const T &lhs, const T &rhs) {
	return length(lhs - rhs);
}

template <class T, EnableIfVector<T>...> auto distanceSq(const T &lhs, const T &rhs) {
	return lengthSq(lhs - rhs);
}

template <class T, EnableIfRealVector<T>...> T normalize(const T &v) { return v / length(v); }

template <class T, EnableIfVector<T, 2>...> auto asXZ(const T &v) {
	using TOut = MakeVector<typename T::Scalar, 3>;
	return TOut(v[0], 0, v[1]);
}
template <class T, EnableIfVector<T, 2>...> auto asXY(const T &v) {
	using TOut = MakeVector<typename T::Scalar, 3>;
	return TOut(v[0], v[1], 0);
}
template <class T, EnableIfVector<T, 2>...> auto asXZY(const T &xz, typename T::Scalar y) {
	using TOut = MakeVector<typename T::Scalar, 3>;
	return TOut(xz[0], y, xz[1]);
}

template <class T, EnableIfVector<T, 3>...> T asXZY(const T &v) { return {v[0], v[2], v[1]}; }

template <class T, EnableIfRealVector<T>...> T vinv(const T &vec) {
	return transform(vec, [](const auto &v) { return inv(v); });
}

// Right-handed coordinate system
template <class T, EnableIfVector<T, 3>...> T cross(const T &a, const T &b) {
	return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

// Default orientation in all vector-related operations
// (cross products, rotations, etc.) is counter-clockwise.

template <class T, EnableIfVector<T, 2>...> auto cross(const T &a, const T &b) {
	return a[0] * b[1] - a[1] * b[0];
}

template <class T, EnableIfVector<T, 2>...> T perpendicular(const T &v) { return {-v[1], v[0]}; }

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

// TODO: remove it?
template <class T, EnableIfRealVector<T>...>
bool areClose(const T &a, const T &b,
			  typename T::Scalar epsilon_sq = constant<typename T::Scalar>::epsilon) {
	return distanceSq(a, b) < epsilon_sq;
}

// TODO: we can't really check it properly for floating-point's...
template <class T, EnableIfRealVector<T>...> bool isNormalized(const T &vec) {
	using Real = typename T::Scalar;
	auto length_sq = lengthSq(vec);
	return length_sq >= Real(1) - constant<Real>::epsilon &&
		   length_sq <= Real(1) + constant<Real>::epsilon;
}

template <class T, EnableIfVector<T>...> bool isZero(const T &vec) { return vec == T(); }

float frand();
float angleDistance(float a, float b);
float blendAngles(float initial, float target, float step);

// Returns angle in range <0, 2 * PI)
float normalizeAngle(float angle);

template <class T, EnableIfRealVector<T>...> bool isnan(const T &v) {
	return anyOf(v.values(), [](auto s) { return isnan(s); });
}

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

template <class T> using Box2 = Box<MakeVector<T, 2>>;
template <class T> using Box3 = Box<MakeVector<T, 3>>;

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

template <class T, int N>
using EnableInDimension = EnableIf<isVector<T, N>(), DisabledInThisDimension>;

DEFINE_ENUM(IsectClass, adjacent, point, segment, none);
using IsectFlags = EnumFlags<IsectClass>;

template <class ValueType = int> struct Hash;
template <class T> int hash(const T &);
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
