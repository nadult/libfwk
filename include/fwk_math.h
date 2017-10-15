// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys_base.h"
#include "fwk_index_range.h"
#include <cmath>
#include <limits>

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

pair<float, float> sincos(float radians);
pair<double, double> sincos(double radians);

bool isnan(float);
bool isnan(double);

struct short2 {
	using Scalar = short;
	enum { vector_size = 2 };

	constexpr short2(short x, short y) : x(x), y(y) {}
	constexpr short2() : x(0), y(0) {}

	explicit constexpr short2(short t) : x(t), y(t) {}

	short2 operator+(const short2 &rhs) const { return short2(x + rhs.x, y + rhs.y); }
	short2 operator-(const short2 &rhs) const { return short2(x - rhs.x, y - rhs.y); }
	short2 operator*(const short2 &rhs) const { return short2(x * rhs.x, y * rhs.y); }
	short2 operator*(short s) const { return short2(x * s, y * s); }
	short2 operator/(short s) const { return short2(x / s, y / s); }
	short2 operator%(short s) const { return short2(x % s, y % s); }
	short2 operator-() const { return short2(-x, -y); }

	short &operator[](int idx) { return v[idx]; }
	const short &operator[](int idx) const { return v[idx]; }

	Span<short, 2> values() { return v; }
	CSpan<short, 2> values() const { return v; }

	FWK_ORDER_BY(short2, x, y)

	union {
		struct {
			short x, y;
		};
		short v[2];
	};
};

struct int2 {
	using Scalar = int;
	enum { vector_size = 2 };

	constexpr int2(short2 rhs) : x(rhs.x), y(rhs.y) {}
	constexpr int2(int x, int y) : x(x), y(y) {}
	constexpr int2() : x(0), y(0) {}

	explicit int2(int t) : x(t), y(t) {}
	explicit operator short2() const { return short2(x, y); }

	int2 operator+(const int2 &rhs) const { return {x + rhs.x, y + rhs.y}; }
	int2 operator-(const int2 &rhs) const { return {x - rhs.x, y - rhs.y}; }
	int2 operator*(const int2 &rhs) const { return {x * rhs.x, y * rhs.y}; }
	int2 operator*(int s) const { return {x * s, y * s}; }
	int2 operator/(int s) const { return {x / s, y / s}; }
	int2 operator%(int s) const { return {x % s, y % s}; }
	int2 operator-() const { return {-x, -y}; }

	int &operator[](int idx) {
		PASSERT(inRange(idx, 0, 2));
		return v[idx];
	}
	const int &operator[](int idx) const {
		PASSERT(inRange(idx, 0, 2));
		return v[idx];
	}

	FWK_ORDER_BY(int2, x, y)

	Span<int, 2> values() { return v; }
	CSpan<int, 2> values() const { return v; }

	union {
		struct {
			int x, y;
		};
		int v[2];
	};
};

struct int3 {
	using Scalar = int;
	enum { vector_size = 3 };

	constexpr int3(int x, int y, int z) : x(x), y(y), z(z) {}
	constexpr int3() : x(0), y(0), z(0) {}

	explicit int3(int t) : x(t), y(t), z(t) {}

	int3 operator+(const int3 &rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
	int3 operator-(const int3 &rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
	int3 operator*(const int3 &rhs) const { return {x * rhs.x, y * rhs.y, z * rhs.z}; }
	int3 operator*(int s) const { return {x * s, y * s, z * s}; }
	int3 operator/(int s) const { return {x / s, y / s, z / s}; }
	int3 operator%(int s) const { return {x % s, y % s, z % s}; }
	int3 operator-() const { return {-x, -y, -z}; }

	int2 xy() const { return {x, y}; }
	int2 xz() const { return {x, z}; }
	int2 yz() const { return {y, z}; }

	int &operator[](int idx) {
		PASSERT(inRange(idx, 0, 3));
		return v[idx];
	}
	const int &operator[](int idx) const {
		PASSERT(inRange(idx, 0, 3));
		return v[idx];
	}

	FWK_ORDER_BY(int3, x, y, z)

	Span<int, 3> values() { return v; }
	CSpan<int, 3> values() const { return v; }

	union {
		struct {
			int x, y, z;
		};
		int v[3];
	};
};

struct int4 {
	using Scalar = int;
	enum { vector_size = 4 };

	constexpr int4(int x, int y, int z, int w) : x(x), y(y), z(z), w(w) {}
	constexpr int4() : x(0), y(0), z(0), w(0) {}

	explicit int4(int t) : x(t), y(t), z(t), w(t) {}

	int4 operator+(const int4 rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; }
	int4 operator-(const int4 rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; }
	int4 operator*(int s) const { return {x * s, y * s, z * s, w * s}; }
	int4 operator/(int s) const { return {x / s, y / s, z / s, w / s}; }
	int4 operator-() const { return {-x, -y, -z, -w}; }

	int &operator[](int idx) {
		PASSERT(inRange(idx, 0, 4));
		return v[idx];
	}
	const int &operator[](int idx) const {
		PASSERT(inRange(idx, 0, 4));
		return v[idx];
	}

	FWK_ORDER_BY(int4, x, y, z, w)

	Span<int, 4> values() { return v; }
	CSpan<int, 4> values() const { return v; }

	union {
		struct {
			int x, y, z, w;
		};
		int v[4];
	};
};

#ifdef FWK_CHECK_NANS
#define CHECK_NANS() DASSERT(!anyOf(v, [](auto s) { return isnan(s); }))
#else
#define CHECK_NANS()
#endif

struct float2 {
	using Scalar = float;
	enum { vector_size = 2 };

	constexpr float2(float x, float y) : x(x), y(y) { CHECK_NANS(); }
	constexpr float2() : x(0.0f), y(0.0f) {}

	explicit float2(float t) : x(t), y(t) {}
	explicit float2(const int2 &vec) : x(vec.x), y(vec.y) {}
	explicit operator short2() const { return {(short)x, (short)y}; }
	explicit operator int2() const { return {(int)x, (int)y}; }

	float2 operator+(const float2 &rhs) const { return {x + rhs.x, y + rhs.y}; }
	float2 operator*(const float2 &rhs) const { return {x * rhs.x, y * rhs.y}; }
	float2 operator/(const float2 &rhs) const { return {x / rhs.x, y / rhs.y}; }
	float2 operator-(const float2 &rhs) const { return {x - rhs.x, y - rhs.y}; }
	float2 operator*(float s) const { return {x * s, y * s}; }
	float2 operator/(float s) const { return *this * (1.0f / s); }
	float2 operator-() const { return {-x, -y}; }

	float &operator[](int idx) {
		PASSERT(inRange(idx, 0, 2));
		return v[idx];
	}
	const float &operator[](int idx) const {
		PASSERT(inRange(idx, 0, 2));
		return v[idx];
	}

	FWK_ORDER_BY(float2, x, y)

	Span<float, 2> values() { return v; }
	CSpan<float, 2> values() const { return v; }

	union {
		struct {
			float x, y;
		};
		float v[2];
	};
};

struct float3 {
	using Scalar = float;
	enum { vector_size = 3 };

	constexpr float3(float x, float y, float z) : x(x), y(y), z(z) { CHECK_NANS(); }
	constexpr float3(const float2 &xy, float z) : x(xy.x), y(xy.y), z(z) { CHECK_NANS(); }
	constexpr float3() : x(0.0f), y(0.0f), z(0.0f) {}

	explicit float3(float t) : x(t), y(t), z(t) {}
	explicit float3(const int3 &vec) : x(vec.x), y(vec.y), z(vec.z) {}
	explicit operator int3() const { return {(int)x, (int)y, (int)z}; }

	float3 operator*(const float3 &rhs) const { return {x * rhs.x, y * rhs.y, z * rhs.z}; }
	float3 operator/(const float3 &rhs) const { return {x / rhs.x, y / rhs.y, z / rhs.z}; }
	float3 operator+(const float3 &rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
	float3 operator-(const float3 &rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
	float3 operator*(float s) const { return {x * s, y * s, z * s}; }
	float3 operator/(float s) const { return *this * (1.0f / s); }
	float3 operator-() const { return {-x, -y, -z}; }

	float &operator[](int idx) {
		PASSERT(inRange(idx, 0, 3));
		return v[idx];
	}
	const float &operator[](int idx) const {
		PASSERT(inRange(idx, 0, 3));
		return v[idx];
	}

	float2 xy() const { return {x, y}; }
	float2 xz() const { return {x, z}; }
	float2 yz() const { return {y, z}; }

	FWK_ORDER_BY(float3, x, y, z)

	Span<float, 3> values() { return v; }
	CSpan<float, 3> values() const { return v; }

	union {
		struct {
			float x, y, z;
		};
		float v[3];
	};
};

struct float4 {
	using Scalar = float;
	enum { vector_size = 4 };

	constexpr float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) { CHECK_NANS(); }
	float4(CSpan<float, 4> v) : float4(v[0], v[1], v[2], v[3]) {}
	constexpr float4(const float3 &xyz, float w) : float4(xyz.x, xyz.y, xyz.z, w) {}
	constexpr float4(const float2 &xy, float z, float w) : float4(xy.x, xy.y, z, w) {}
	constexpr float4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

	explicit float4(float t) : float4(t, t, t, t) {}
	explicit float4(const int4 &vec) : x(vec.x), y(vec.y), z(vec.z), w(vec.w) {}
	explicit operator int4() const { return {(int)x, (int)y, (int)z, (int)w}; }

	float4 operator*(const float4 &rhs) const {
		return {x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w};
	}
	float4 operator/(const float4 &rhs) const {
		return {x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w};
	}
	float4 operator+(const float4 &rhs) const {
		return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w};
	}
	float4 operator-(const float4 &rhs) const {
		return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w};
	}
	float4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
	float4 operator/(float s) const { return *this * (1.0f / s); }
	float4 operator-() const { return {-x, -y, -z, -w}; }

	float &operator[](int idx) {
		PASSERT(inRange(idx, 0, 4));
		return v[idx];
	}
	const float &operator[](int idx) const {
		PASSERT(inRange(idx, 0, 4));
		return v[idx];
	}

	float2 xy() const { return {x, y}; }
	float2 xz() const { return {x, z}; }
	float2 yz() const { return {y, z}; }
	float3 xyz() const { return {x, y, z}; }

	FWK_ORDER_BY(float4, x, y, z, w)

	Span<float, 4> values() { return v; }
	CSpan<float, 4> values() const { return v; }

	// TODO: when adding support for SSE, make sure to also write
	// default constructors and operator=, becuase compiler might have some trouble
	// when optimizing
	// ALSO: verify if it is really required
	union {
		struct {
			float x, y, z, w;
		};
		float v[4];
	};
};

struct double2 {
	using Scalar = double;
	enum { vector_size = 2 };

	constexpr double2(double x, double y) : x(x), y(y) { CHECK_NANS(); }
	constexpr double2() : x(0.0f), y(0.0f) {}

	double2(const int2 &vec) : x(vec.x), y(vec.y) {}
	explicit double2(double t) : double2(t, t) {}
	explicit double2(const float2 &vec) : double2(vec.x, vec.y) {}
	explicit operator int2() const { return {(int)x, (int)y}; }
	explicit operator short2() const { return {(short)x, (short)y}; }
	explicit operator float2() const { return {(float)x, (float)y}; }

	double2 operator+(const double2 &rhs) const { return double2(x + rhs.x, y + rhs.y); }
	double2 operator*(const double2 &rhs) const { return double2(x * rhs.x, y * rhs.y); }
	double2 operator/(const double2 &rhs) const { return double2(x / rhs.x, y / rhs.y); }
	double2 operator-(const double2 &rhs) const { return double2(x - rhs.x, y - rhs.y); }
	double2 operator*(double s) const { return double2(x * s, y * s); }
	double2 operator/(double s) const { return *this * (1.0f / s); }
	double2 operator-() const { return double2(-x, -y); }

	double &operator[](int idx) {
		PASSERT(inRange(idx, 0, 2));
		return v[idx];
	}
	const double &operator[](int idx) const {
		PASSERT(inRange(idx, 0, 2));
		return v[idx];
	}

	FWK_ORDER_BY(double2, x, y)

	Span<double, 2> values() { return v; }
	CSpan<double, 2> values() const { return v; }

	union {
		struct {
			double x, y;
		};
		double v[2];
	};
};

struct double3 {
	using Scalar = double;
	enum { vector_size = 3 };

	constexpr double3(double x, double y, double z) : x(x), y(y), z(z) { CHECK_NANS(); }
	constexpr double3(const double2 &xy, double z) : double3(xy.x, xy.y, z) {}
	constexpr double3() : x(0.0f), y(0.0f), z(0.0f) {}

	double3(const int3 &vec) : x(vec.x), y(vec.y), z(vec.z) {}
	explicit double3(double t) : double3(t, t, t) {}
	explicit double3(const float3 &vec) : double3(vec.x, vec.y, vec.z) {}
	explicit operator int3() const { return {(int)x, (int)y, (int)z}; }
	explicit operator float3() const { return {(float)x, (float)y, (float)z}; }

	double3 operator*(const double3 &rhs) const { return double3(x * rhs.x, y * rhs.y, z * rhs.z); }
	double3 operator/(const double3 &rhs) const { return double3(x / rhs.x, y / rhs.y, z / rhs.z); }
	double3 operator+(const double3 &rhs) const { return double3(x + rhs.x, y + rhs.y, z + rhs.z); }
	double3 operator-(const double3 &rhs) const { return double3(x - rhs.x, y - rhs.y, z - rhs.z); }
	double3 operator*(double s) const { return double3(x * s, y * s, z * s); }
	double3 operator/(double s) const { return *this * (1.0f / s); }
	double3 operator-() const { return double3(-x, -y, -z); }

	double &operator[](int idx) {
		PASSERT(inRange(idx, 0, 3));
		return v[idx];
	}
	const double &operator[](int idx) const {
		PASSERT(inRange(idx, 0, 3));
		return v[idx];
	}

	double2 xy() const { return double2(x, y); }
	double2 xz() const { return double2(x, z); }
	double2 yz() const { return double2(y, z); }

	FWK_ORDER_BY(double3, x, y, z)

	Span<double, 3> values() { return v; }
	CSpan<double, 3> values() const { return v; }

	union {
		struct {
			double x, y, z;
		};
		double v[3];
	};
};

struct double4 {
	using Scalar = double;
	enum { vector_size = 4 };

	double4(CSpan<double, 4> v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {}
	constexpr double4(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {
		CHECK_NANS();
	}
	explicit constexpr double4(double t) : double4(t, t, t, t) {}
	constexpr double4(const double3 &xyz, double w) : double4(xyz.x, xyz.y, xyz.z, w) {}
	constexpr double4(const double2 &xy, double z, double w) : double4(xy.x, xy.y, z, w) {}
	constexpr double4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

	double4(const int4 &vec) : x(vec.x), y(vec.y), z(vec.z), w(vec.w) {}
	explicit double4(const float4 &vec) : double4(vec.x, vec.y, vec.z, vec.w) {}
	explicit operator int4() const { return {(int)x, (int)y, (int)z, (int)w}; }
	explicit operator float4() const { return {(float)x, (float)y, (float)z, (float)w}; }

	double4 operator*(const double4 &rhs) const {
		return double4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
	}
	double4 operator/(const double4 &rhs) const {
		return double4(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w);
	}
	double4 operator+(const double4 &rhs) const {
		return double4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
	}
	double4 operator-(const double4 &rhs) const {
		return double4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
	}
	double4 operator*(double s) const { return double4(x * s, y * s, z * s, w * s); }
	double4 operator/(double s) const { return *this * (1.0f / s); }
	double4 operator-() const { return double4(-x, -y, -z, -w); }

	double &operator[](int idx) {
		PASSERT(inRange(idx, 0, 4));
		return v[idx];
	}
	const double &operator[](int idx) const {
		PASSERT(inRange(idx, 0, 4));
		return v[idx];
	}

	double2 xy() const { return double2(x, y); }
	double2 xz() const { return double2(x, z); }
	double2 yz() const { return double2(y, z); }
	double3 xyz() const { return double3(x, y, z); }

	FWK_ORDER_BY(double4, x, y, z, w)

	Span<double, 4> values() { return v; }
	CSpan<double, 4> values() const { return v; }

	// TODO: when adding support for SSE, make sure to also write
	// default constructors and operator=, becuase compiler might have some trouble
	// when optimizing
	// ALSO: verify if it is really required
	union {
		struct {
			double x, y, z, w;
		};
		double v[4];
	};
};

#undef CHECK_NANS

struct NotAReal;
struct NotAIntegral;
struct NotAScalar;
struct NotAMathObject;
template <int N> struct NotAValidVector;

namespace detail {

	template <class T> struct IsIntegral : public std::is_integral<T> {};
	template <class T> struct IsRational : public std::false_type {};
	template <class T> struct IsReal : public std::is_floating_point<T> {};

	template <class T> struct IsScalar {
		enum { value = IsReal<T>::value || IsIntegral<T>::value || IsRational<T>::value };
	};

	template <class T, template <class> class Trait> struct ScalarTrait {
		template <class U>
		static typename std::enable_if<Trait<typename U::Scalar>::value, char>::type test(U *);
		template <class U> static long test(...);
		enum { value = sizeof(test<T>(nullptr)) == 1 };
	};

	// TODO: Maybe this is not necessary, just specify all classes which should be treated
	// as vectors or other math objects (similarly to MakeVector)

	template <class T, int N> struct VectorInfo {
		template <class U> struct RetOk {
			using Scalar = typename U::Scalar;
			using Vector = U;
			template <class A> using Arg = A;

			enum {
				size = U::vector_size,
				is_real = IsReal<Scalar>::value,
				is_integral = IsIntegral<Scalar>::value,
				is_rational = IsRational<Scalar>::value
			};
		};
		using RetError = NotAValidVector<N>;
		struct Zero {
			enum { size = 0, is_real = 0, is_integral = 0, is_rational = 0 };
		};

		template <class U>
		static constexpr auto test(int) ->
			typename std::conditional<(sizeof(typename U::Scalar) > 0) &&
										  (U::vector_size == N || N == 0),
									  RetOk<U>, RetError>::type;
		template <class U> static constexpr RetError test(...);

		using Ret = decltype(test<T>(0));
		using Get =
			typename std::conditional<std::is_same<Ret, RetError>::value, Zero, RetOk<T>>::type;
	};

	template <class T, int N = 0> struct IsVector {
		enum { value = VectorInfo<T, N>::size > 0 };
	};

	// TODO: better name? also differentiate from fwk::vector
	template <class T, int N> struct MakeVector { using type = NotAValidVector<0>; };
	template <> struct MakeVector<short, 2> { using type = short2; };
	template <> struct MakeVector<int, 2> { using type = int2; };
	template <> struct MakeVector<int, 3> { using type = int3; };
	template <> struct MakeVector<int, 4> { using type = int4; };

	template <> struct MakeVector<float, 2> { using type = float2; };
	template <> struct MakeVector<float, 3> { using type = float3; };
	template <> struct MakeVector<float, 4> { using type = float4; };
	template <> struct MakeVector<double, 2> { using type = double2; };
	template <> struct MakeVector<double, 3> { using type = double3; };
	template <> struct MakeVector<double, 4> { using type = double4; };

	template <class T> struct GetScalar {
		template <class U>
		static auto test(U *) -> typename std::enable_if<IsScalar<U>::value, U>::type;
		template <class U> static auto test(U *) -> typename U::Scalar;
		template <class U> static void test(...);
		using type = decltype(test<T>(nullptr));
	};
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

template <class T, int N = 0> constexpr bool isVector() {
	return detail::VectorInfo<T, N>::Get::size > 0;
}
template <class T, int N = 0> constexpr bool isRealVector() {
	return detail::VectorInfo<T, N>::Get::size > 0 && detail::VectorInfo<T, N>::Get::is_real;
}
template <class T, int N = 0> constexpr bool isRationalOrRealVector() {
	using VecInfo = typename detail::VectorInfo<T, N>::Get;
	return VecInfo::size > 0 && (VecInfo::is_rational || VecInfo::is_real);
}
template <class T, int N = 0> constexpr bool isIntegralVector() {
	return detail::VectorInfo<T, N>::Get::size > 0 && detail::VectorInfo<T, N>::Get::is_integral;
}
template <class T, int N = 0> constexpr bool isRationalVector() {
	return detail::VectorInfo<T, N>::Get::size > 0 && detail::VectorInfo<T, N>::Get::is_rational;
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

template <class T, int N>
using MakeVector = typename detail::MakeVector<typename detail::GetScalar<T>::type, N>::type;

template <class T> using Vector2 = MakeVector<T, 2>;
template <class T> using Vector3 = MakeVector<T, 3>;
template <class T> using Vector4 = MakeVector<T, 4>;

template <class T> struct ToReal { using type = double; };
template <> struct ToReal<float> { using type = float; };

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

inline double inv(double s) { return 1.0 / s; }
inline float inv(float s) { return 1.0f / s; }

using std::ceil;
using std::floor;

inline int abs(int s) { return std::abs(s); }
inline double abs(double s) { return std::abs(s); }
inline float abs(float s) { return std::abs(s); }

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

template <class T, EnableIfVector<T, 2>...> Vector3<T> asXZ(const T &v) { return {v[0], 0, v[1]}; }
template <class T, EnableIfVector<T, 2>...> Vector3<T> asXY(const T &v) { return {v[0], v[1], 0}; }
template <class T, EnableIfVector<T, 2>...> Vector3<T> asXZY(const T &xz, typename T::Scalar y) {
	return {xz[0], y, xz[1]};
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
