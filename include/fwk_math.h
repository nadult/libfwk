/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#ifndef FWK_MATH_H
#define FWK_MATH_H

#include "fwk_base.h"
#include <cmath>
#include <limits>
#include <random>

namespace fwk {

using llint = long long;
using qint = __int128_t;

#define FWK_VEC_RANGE()                                                                            \
	auto begin() { return v; }                                                                     \
	auto end() { return v + arraySize(v); }                                                        \
	auto begin() const { return v; }                                                               \
	auto end() const { return v + arraySize(v); }                                                  \
	auto data() { return v; }                                                                      \
	auto data() const { return v; }                                                                \
	constexpr int size() const { return arraySize(v); }

// TODO: make sure that all classes / structures here have proper default constructor (for
// example AxisAngle requires fixing)

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

	FWK_ORDER_BY(short2, x, y)
	FWK_VEC_RANGE()

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

	int &operator[](int idx) { return v[idx]; }
	const int &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(int2, x, y)
	FWK_VEC_RANGE()

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

	int &operator[](int idx) { return v[idx]; }
	const int &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(int3, x, y, z)
	FWK_VEC_RANGE()

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

	int &operator[](int idx) { return v[idx]; }
	const int &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(int4, x, y, z, w)
	FWK_VEC_RANGE()

	union {
		struct {
			int x, y, z, w;
		};
		int v[4];
	};
};

struct llint2 {
	using Scalar = llint;
	enum { vector_size = 2 };

	constexpr llint2(short2 rhs) : x(rhs.x), y(rhs.y) {}
	constexpr llint2(int2 rhs) : x(rhs.x), y(rhs.y) {}
	constexpr llint2(llint x, llint y) : x(x), y(y) {}
	constexpr llint2() : x(0), y(0) {}

	explicit llint2(int t) : x(t), y(t) {}
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

	constexpr qint2(short2 rhs) : x(rhs.x), y(rhs.y) {}
	constexpr qint2(int2 rhs) : x(rhs.x), y(rhs.y) {}
	constexpr qint2(llint2 rhs) : x(rhs.x), y(rhs.y) {}
	constexpr qint2(qint x, qint y) : x(x), y(y) {}
	constexpr qint2() : x(0), y(0) {}

	explicit qint2(int t) : x(t), y(t) {}
	explicit operator short2() const { return short2(x, y); }
	explicit operator int2() const { return int2(x, y); }
	explicit operator llint2() const { return llint2(x, y); }

	qint2 operator+(const qint2 &rhs) const { return {x + rhs.x, y + rhs.y}; }
	qint2 operator-(const qint2 &rhs) const { return {x - rhs.x, y - rhs.y}; }
	qint2 operator*(const qint2 &rhs) const { return {x * rhs.x, y * rhs.y}; }
	qint2 operator*(int s) const { return {x * s, y * s}; }
	qint2 operator/(int s) const { return {x / s, y / s}; }
	qint2 operator%(int s) const { return {x % s, y % s}; }
	qint2 operator-() const { return {-x, -y}; }

	qint &operator[](int idx) { return v[idx]; }
	const qint &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(qint2, x, y)
	FWK_VEC_RANGE()

	union {
		struct {
			qint x, y;
		};
		qint v[2];
	};
};

struct float2 {
	using Scalar = float;
	enum { vector_size = 2 };

	constexpr float2(float x, float y) : x(x), y(y) {}
	constexpr float2() : x(0.0f), y(0.0f) {}

	explicit float2(float t) : x(t), y(t) {}
	explicit float2(const int2 &vec) : x(vec.x), y(vec.y) {}
	explicit operator int2() const { return {(int)x, (int)y}; }

	float2 operator+(const float2 &rhs) const { return {x + rhs.x, y + rhs.y}; }
	float2 operator*(const float2 &rhs) const { return {x * rhs.x, y * rhs.y}; }
	float2 operator/(const float2 &rhs) const { return {x / rhs.x, y / rhs.y}; }
	float2 operator-(const float2 &rhs) const { return {x - rhs.x, y - rhs.y}; }
	float2 operator*(float s) const { return {x * s, y * s}; }
	float2 operator/(float s) const { return *this * (1.0f / s); }
	float2 operator-() const { return {-x, -y}; }

	float &operator[](int idx) { return v[idx]; }
	const float &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(float2, x, y)
	FWK_VEC_RANGE()

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

	constexpr float3(float x, float y, float z) : x(x), y(y), z(z) {}
	constexpr float3(const float2 &xy, float z) : x(xy.x), y(xy.y), z(z) {}
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

	float &operator[](int idx) { return v[idx]; }
	const float &operator[](int idx) const { return v[idx]; }

	float2 xy() const { return {x, y}; }
	float2 xz() const { return {x, z}; }
	float2 yz() const { return {y, z}; }

	FWK_ORDER_BY(float3, x, y, z)
	FWK_VEC_RANGE()

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

	float4(CRange<float, 4> v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {}
	constexpr float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	constexpr float4(const float3 &xyz, float w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
	constexpr float4(const float2 &xy, float z, float w) : x(xy.x), y(xy.y), z(z), w(w) {}
	constexpr float4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

	explicit float4(float t) : x(t), y(t), z(t), w(t) {}
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

	float &operator[](int idx) { return v[idx]; }
	const float &operator[](int idx) const { return v[idx]; }

	float2 xy() const { return {x, y}; }
	float2 xz() const { return {x, z}; }
	float2 yz() const { return {y, z}; }
	float3 xyz() const { return {x, y, z}; }

	FWK_ORDER_BY(float4, x, y, z, w)
	FWK_VEC_RANGE()

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

	constexpr double2(double x, double y) : x(x), y(y) {}
	constexpr double2() : x(0.0f), y(0.0f) {}

	double2(const int2 &vec) : x(vec.x), y(vec.y) {}
	explicit double2(double t) : x(t), y(t) {}
	explicit double2(const float2 &vec) : x(vec.x), y(vec.y) {}
	explicit operator int2() const { return int2((int)x, (int)y); }
	explicit operator float2() const { return {(float)x, (float)y}; }

	double2 operator+(const double2 &rhs) const { return double2(x + rhs.x, y + rhs.y); }
	double2 operator*(const double2 &rhs) const { return double2(x * rhs.x, y * rhs.y); }
	double2 operator/(const double2 &rhs) const { return double2(x / rhs.x, y / rhs.y); }
	double2 operator-(const double2 &rhs) const { return double2(x - rhs.x, y - rhs.y); }
	double2 operator*(double s) const { return double2(x * s, y * s); }
	double2 operator/(double s) const { return *this * (1.0f / s); }
	double2 operator-() const { return double2(-x, -y); }

	double &operator[](int idx) { return v[idx]; }
	const double &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(double2, x, y)
	FWK_VEC_RANGE()

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

	constexpr double3(double x, double y, double z) : x(x), y(y), z(z) {}
	constexpr double3(const double2 &xy, double z) : x(xy.x), y(xy.y), z(z) {}
	constexpr double3() : x(0.0f), y(0.0f), z(0.0f) {}

	double3(const int3 &vec) : x(vec.x), y(vec.y), z(vec.z) {}
	explicit double3(double t) : x(t), y(t), z(t) {}
	explicit double3(const float3 &vec) : x(vec.x), y(vec.y), z(vec.z) {}
	explicit operator int3() const { return {(int)x, (int)y, (int)z}; }
	explicit operator float3() const { return {(float)x, (float)y, (float)z}; }

	double3 operator*(const double3 &rhs) const { return double3(x * rhs.x, y * rhs.y, z * rhs.z); }
	double3 operator/(const double3 &rhs) const { return double3(x / rhs.x, y / rhs.y, z / rhs.z); }
	double3 operator+(const double3 &rhs) const { return double3(x + rhs.x, y + rhs.y, z + rhs.z); }
	double3 operator-(const double3 &rhs) const { return double3(x - rhs.x, y - rhs.y, z - rhs.z); }
	double3 operator*(double s) const { return double3(x * s, y * s, z * s); }
	double3 operator/(double s) const { return *this * (1.0f / s); }
	double3 operator-() const { return double3(-x, -y, -z); }

	double &operator[](int idx) { return v[idx]; }
	const double &operator[](int idx) const { return v[idx]; }

	double2 xy() const { return double2(x, y); }
	double2 xz() const { return double2(x, z); }
	double2 yz() const { return double2(y, z); }

	FWK_ORDER_BY(double3, x, y, z)
	FWK_VEC_RANGE()

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

	double4(CRange<double, 4> v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {}
	constexpr double4(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {}
	explicit constexpr double4(double t) : x(t), y(t), z(t), w(t) {}
	constexpr double4(const double3 &xyz, double w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
	constexpr double4(const double2 &xy, double z, double w) : x(xy.x), y(xy.y), z(z), w(w) {}
	constexpr double4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

	double4(const int4 &vec) : x(vec.x), y(vec.y), z(vec.z), w(vec.w) {}
	explicit double4(const float4 &vec) : x(vec.x), y(vec.y), z(vec.z), w(vec.w) {}
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

	double &operator[](int idx) { return v[idx]; }
	const double &operator[](int idx) const { return v[idx]; }

	double2 xy() const { return double2(x, y); }
	double2 xz() const { return double2(x, z); }
	double2 yz() const { return double2(y, z); }
	double3 xyz() const { return double3(x, y, z); }

	FWK_ORDER_BY(double4, x, y, z, w)
	FWK_VEC_RANGE()

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

struct NotAReal;
struct NotAIntegral;
struct NotAScalar;
struct NotAMathObject;
template <int N> struct NotAVector;

namespace detail {

	template <class T> struct IsIntegral : public std::is_integral<T> {};
	template <> struct IsIntegral<qint> : public std::true_type {};

	template <class T> using IsReal = std::is_floating_point<T>;
	template <class T> struct IsScalar {
		enum { value = std::is_floating_point<T>::value || IsIntegral<T>::value };
	};

	template <class T> struct IsRealObject {
		template <class U>
		static typename std::enable_if<IsReal<typename U::Scalar>::value, char>::type test(U *);
		template <class U> static long test(...);
		enum { value = sizeof(test<T>(nullptr)) == 1 };
	};

	template <class T> struct IsIntegralObject {
		template <class U>
		static typename std::enable_if<IsIntegral<typename U::Scalar>::value, char>::type test(U *);
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
				is_real = std::is_floating_point<Scalar>::value,
				is_integral = std::is_integral<Scalar>::value
			};
		};
		using RetError = NotAVector<N>;
		struct Zero {
			enum { size = 0, is_real = 0, is_integral = 0 };
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
	template <class T, int N> struct MakeVector { using type = NotAVector<0>; };
	template <> struct MakeVector<short, 2> { using type = short2; };
	template <> struct MakeVector<int, 2> { using type = int2; };
	template <> struct MakeVector<int, 3> { using type = int3; };
	template <> struct MakeVector<int, 4> { using type = int4; };
	template <> struct MakeVector<llint, 2> { using type = llint2; };
	template <> struct MakeVector<qint, 2> { using type = qint2; };

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
template <class T> constexpr bool isScalar() { return detail::IsScalar<T>::value; }

template <class T> constexpr bool isRealObject() { return detail::IsRealObject<T>::value; }
template <class T> constexpr bool isIntegralObject() { return detail::IsIntegralObject<T>::value; }
template <class T> constexpr bool isMathObject() {
	return isRealObject<T>() || isIntegralObject<T>();
}
template <class T, int N = 0> constexpr bool isVector() {
	return detail::VectorInfo<T, N>::Get::size > 0;
}
template <class T, int N = 0> constexpr bool isRealVector() {
	return detail::VectorInfo<T, N>::Get::size > 0 && detail::VectorInfo<T, N>::Get::is_real;
}
template <class T, int N = 0> constexpr bool isIntegralVector() {
	return detail::VectorInfo<T, N>::Get::size > 0 && detail::VectorInfo<T, N>::Get::is_integral;
}

template <class T> using EnableIfScalar = EnableIf<isScalar<T>(), NotAScalar>;
template <class T> using EnableIfReal = EnableIf<std::is_floating_point<T>::value, NotAReal>;
template <class T> using EnableIfIntegral = EnableIf<std::is_integral<T>::value, NotAIntegral>;

template <class T> using EnableIfMathObject = EnableIf<isMathObject<T>(), NotAMathObject>;

template <class T, int N = 0> using EnableIfVector = EnableIf<isVector<T, N>(), NotAVector<N>>;
template <class T, int N = 0>
using EnableIfRealVector = EnableIf<isRealVector<T, N>(), NotAVector<N>>;
template <class T, int N = 0>
using EnableIfIntegralVector = EnableIf<isIntegralVector<T, N>(), NotAVector<N>>;

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

template <class T, EnableIfVector<T>...> pair<T, T> vecMinMax(CRange<T> range) {
	if(range.empty())
		return make_pair(T(), T());
	T tmin = range[0], tmax = range[0];
	for(int n = 1; n < range.size(); n++) {
		tmin = vmin(tmin, range[n]);
		tmax = vmax(tmax, range[n]);
	}
	return make_pair(tmin, tmax);
}

template <class TRange, class T = typename ContainerBaseType<TRange>::type, EnableIfVector<T>...>
pair<T, T> vecMinMax(const TRange &range) {
	return vecMinMax(makeRange(range));
}

template <class T, EnableIfRealVector<T>...> T vfloor(T vec) {
	for(int n = 0; n < T::vector_size; n++)
		vec[n] = std::floor(vec[n]);
	return vec;
}

template <class T, EnableIfRealVector<T>...> T vceil(T vec) {
	for(int n = 0; n < T::vector_size; n++)
		vec[n] = std::ceil(vec[n]);
	return vec;
}

template <class T, EnableIfVector<T, 2>...> auto dot(const T &lhs, const T &rhs) {
	return lhs.x * rhs.x + lhs.y * rhs.y;
}

template <class T, EnableIfVector<T, 3>...> auto dot(const T &lhs, const T &rhs) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

template <class T, EnableIfVector<T, 4>...> auto dot(const T &lhs, const T &rhs) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
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

template <class T, EnableIfVector<T, 2>...> T vabs(const T &v) {
	return {std::abs(v.x), std::abs(v.y)};
}

template <class T, EnableIfVector<T, 3>...> T vabs(const T &v) {
	return {std::abs(v.x), std::abs(v.y), std::abs(v.z)};
}

template <class T, EnableIfVector<T, 4>...> T vabs(const T &v) {
	return T(std::abs(v.x), std::abs(v.y), std::abs(v.z), std::abs(v.w));
}

template <class T, EnableIfVector<T, 2>...> Vector3<T> asXZ(const T &v) { return {v[0], 0, v[1]}; }
template <class T, EnableIfVector<T, 2>...> Vector3<T> asXY(const T &v) { return {v[0], v[1], 0}; }
template <class T, EnableIfVector<T, 2>...> Vector3<T> asXZY(const T &xz, typename T::Scalar y) {
	return {xz[0], y, xz[1]};
}

template <class T, EnableIfVector<T, 3>...> T asXZY(const T &v) { return {v[0], v[2], v[1]}; }

template <class T, EnableIfRealVector<T, 2>...> T inv(const T &v) {
	using Scalar = typename T::Scalar;
	return {Scalar(1) / v.x, Scalar(1) / v.y};
}
template <class T, EnableIfRealVector<T, 3>...> T inv(const T &v) {
	using Scalar = typename T::Scalar;
	return {Scalar(1) / v.x, Scalar(1) / v.y, Scalar(1) / v.z};
}
template <class T, EnableIfRealVector<T, 4>...> T inv(const T &v) {
	using Scalar = typename T::Scalar;
	return {Scalar(1) / v.x, Scalar(1) / v.y, Scalar(1) / v.z, Scalar(1) / v.w};
}

// Right-handed coordinate system
template <class T, EnableIfVector<T, 3>...> T cross(const T &a, const T &b) {
	return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

// Default orientation in all vector-related operations
// (cross products, rotations, etc.) is counter-clockwise.

template <class T, EnableIfVector<T, 2>...> auto cross(const T &a, const T &b) {
	return a.x * b.y - a.y * b.x;
}

template <class T, EnableIfVector<T, 2>...> T perpendicular(const T &v) { return {-v.y, v.x}; }

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

bool isnan(float);
bool isnan(double);

template <class T, EnableIfRealVector<T>...> bool isnan(const T &v) {
	return anyOf(v, [](auto s) { return isnan(s); });
}

class Matrix3;
class Matrix4;

struct DisabledInThisDimension;

template <class T, int N>
using EnableInDimension = EnableIf<isVector<T, N>(), DisabledInThisDimension>;
#define ENABLE_IF_SIZE(n) template <class U = Vector, EnableInDimension<U, n>...>

// Axis-aligned box (or rect in 2D case)
// Invariant: min <= max (use validRange)
template <class T> class Box {
	using NoAsserts = detail::NoAssertsTag;
	Box(T min, T max, NoAsserts) : m_min(min), m_max(max) {}

  public:
	static_assert(isVector<T>(), "");

	using Scalar = typename T::Scalar;
	using Vector = T;
	using Vector2 = fwk::Vector2<Scalar>;
	using Point = Vector;

	enum { dim_size = Vector::vector_size, num_corners = 1 << dim_size };

	// min <= max in all dimensions; can be empty
	bool validRange(const Point &min, const Point &max) const {
		for(int i = 0; i < dim_size; i++)
			if(!(min[i] <= max[i]))
				return false;
		return true;
	}

	// min >= max in any dimension
	bool emptyRange(const Point &min, const Point &max) const {
		for(int n = 0; n < dim_size; n++)
			if(!(min[n] < max[n]))
				return true;
		return false;
	}

	ENABLE_IF_SIZE(2)
	explicit Box(Scalar min_x, Scalar min_y, Scalar max_x, Scalar max_y)
		: Box({min_x, min_y}, {max_x, max_y}) {}
	ENABLE_IF_SIZE(3)
	explicit Box(Scalar min_x, Scalar min_y, Scalar min_z, Scalar max_x, Scalar max_y, Scalar max_z)
		: Box({min_x, min_y, min_z}, {max_x, max_y, max_z}) {}

	Box(Point min, Point max) : m_min(min), m_max(max) { DASSERT(validRange(min, max)); }
	explicit Box(Vector size) : Box(Vector(), size) {}
	Box(const pair<Vector, Vector> &min_max) : Box(min_max.first, min_max.second) {}
	Box() : m_min(), m_max() {}

	template <class TVector>
	explicit Box(const Box<TVector> &irect) : Box(Vector(irect.min()), Vector(irect.max())) {}

	Scalar min(int i) const { return m_min[i]; }
	Scalar max(int i) const { return m_max[i]; }

	const Point &min() const { return m_min; }
	const Point &max() const { return m_max; }

	Scalar x() const { return m_min[0]; }
	Scalar y() const { return m_min[1]; }
	ENABLE_IF_SIZE(3) Scalar z() const { return m_min[2]; }

	Scalar ex() const { return m_max[0]; }
	Scalar ey() const { return m_max[1]; }
	ENABLE_IF_SIZE(3) Scalar ez() const { return m_max[2]; }

	Scalar width() const { return size(0); }
	Scalar height() const { return size(1); }
	ENABLE_IF_SIZE(3) Scalar depth() const { return size(2); }

	ENABLE_IF_SIZE(2) Scalar surfaceArea() const { return width() * height(); }
	ENABLE_IF_SIZE(3) Scalar volume() const { return width() * height() * depth(); }

	Scalar size(int axis) const { return m_max[axis] - m_min[axis]; }
	Vector size() const { return m_max - m_min; }
	Point center() const { return (m_max + m_min) / Scalar(2); }

	Box operator+(const Vector &offset) const { return Box(m_min + offset, m_max + offset); }
	Box operator-(const Vector &offset) const { return Box(m_min - offset, m_max - offset); }
	Box operator*(const Vector &scale) const {
		Vector tmin = m_min * scale, tmax = m_max * scale;
		for(int n = 0; n < dim_size; n++)
			if(scale[n] < Scalar(0))
				swap(tmin[n], tmax[n]);
		return {tmin, tmax, NoAsserts()};
	}

	Box operator*(Scalar scale) const {
		auto tmin = m_min * scale, tmax = m_max * scale;
		if(scale < Scalar(0))
			swap(tmin, tmax);
		return {tmin, tmax, NoAsserts()};
	}

	bool empty() const { return emptyRange(m_min, m_max); }

	bool contains(const Point &point) const {
		for(int i = 0; i < dim_size; i++)
			if(!(point[i] >= m_min[i] && point[i] <= m_max[i]))
				return false;
		return true;
	}

	bool contains(const Box &box) const { return box == intersection(box); }

	ENABLE_IF_SIZE(2) bool containsPixel(const T &pos) const {
		for(int i = 0; i < dim_size; i++)
			if(!(pos[i] >= m_min[i] && pos[i] + Scalar(1) <= m_max[i]))
				return false;
		return true;
	}

	ENABLE_IF_SIZE(2) bool pixelCount(int axis) const {
		return max(size(axis) - Scalar(1), Scalar(0));
	}

	ENABLE_IF_SIZE(2) T pixelCount() const {
		return vmax(size() - Vector(Scalar(1)), T(Scalar(0)));
	}

	ENABLE_IF_SIZE(2) array<Point, 4> corners() const {
		return {{m_min, {m_min.x, m_max.y}, m_max, {m_max.x, m_min.y}}};
	}

	ENABLE_IF_SIZE(3) array<Point, num_corners> corners() const {
		array<Vector, num_corners> out;
		for(int n = 0; n < num_corners; n++)
			for(int i = 0; i < dim_size; i++) {
				int bit = 1 << (dim_size - i - 1);
				out[n][i] = (n & bit ? m_min : m_max)[i];
			}
		return out;
	}

	Box intersectionOrEmpty(const Box &rhs) const {
		auto tmin = vmax(m_min, rhs.m_min);
		auto tmax = vmin(m_max, rhs.m_max);

		if(!emptyRange(tmin, tmax))
			return Box{tmin, tmax, NoAsserts()};
		return Box{};
	}

	// When Boxes touch, returned Box will be empty
	Maybe<Box> intersection(const Box &rhs) const {
		auto tmin = vmax(m_min, rhs.m_min);
		auto tmax = vmin(m_max, rhs.m_max);

		if(!validRange(tmin, tmax))
			return none;
		return Box{tmin, tmax, NoAsserts()};
	}

	Point closestPoint(const Point &point) const { return vmin(vmax(point, m_min), m_max); }

	Scalar distanceSq(const Point &point) const {
		return fwk::distanceSq(point, closestPoint(point));
	}

	auto distanceSq(const Box &rhs) const {
		Point p1 = vclamp(rhs.center(), m_min, m_max);
		Point p2 = vclamp(p1, rhs.m_min, rhs.m_max);
		return fwk::distanceSq(p1, p2);
	}

	auto distance(const Point &point) const { return std::sqrt(distanceSq(point)); }
	auto distance(const Box &box) const { return std::sqrt(distanceSq(box)); }

	Box inset(const Vector &val_min, const Vector &val_max) const {
		auto new_min = m_min + val_min, new_max = m_max - val_max;
		return {vmin(new_min, new_max), vmax(new_min, new_max), NoAsserts()};
	}
	Box inset(const Vector &value) const { return inset(value, value); }
	Box inset(Scalar value) const { return inset(Vector(value)); }

	Box enlarge(const Vector &val_min, const Vector &val_max) const {
		return inset(-val_min, -val_max);
	}
	Box enlarge(const Vector &value) const { return inset(-value); }
	Box enlarge(Scalar value) const { return inset(Vector(-value)); }

	FWK_ORDER_BY(Box, m_min, m_max);

	auto begin() const { return m_v; }
	auto end() const { return m_v + arraySize(m_v); }

	ENABLE_IF_SIZE(3) Box<Vector2> xz() const { return {m_min.xz(), m_max.xz()}; }
	ENABLE_IF_SIZE(3) Box<Vector2> xy() const { return {m_min.xy(), m_max.xy()}; }
	ENABLE_IF_SIZE(3) Box<Vector2> yz() const { return {m_min.yz(), m_max.yz()}; }

  private:
	union {
		struct {
			Point m_min, m_max;
		};
		Point m_v[2];
	};
};

using IRect = Box<int2>;
using FRect = Box<float2>;
using DRect = Box<double2>;
using IBox = Box<int3>;
using FBox = Box<float3>;
using DBox = Box<double3>;

template <class TRange, EnableIfRange<TRange>..., EnableIfVector<RangeBase<TRange>>...>
auto enclose(TRange points) -> Box<RangeBase<TRange>> {
	return {vecMinMax(points)};
}

template <class T, EnableIfRealVector<T>...> auto encloseIntegral(const Box<T> &box) {
	using IVec = MakeVector<int, T::vector_size>;
	T min = vfloor(box.min());
	T max = vceil(box.max());
	return Box<IVec>{IVec(min), IVec(max)};
}

template <class T> Box<T> enclose(const Box<T> &lhs, const Box<T> &rhs) {
	return {vmin(lhs.min(), rhs.min()), vmax(lhs.max(), rhs.max())};
}

template <class T> Box<T> encloseNotEmpty(const Box<T> &lhs, const Box<T> &rhs) {
	return lhs.empty() ? rhs : rhs.empty() ? lhs : enclose(lhs, rhs);
}

template <class T> Box<T> enclose(const Box<T> &box, const T &point) {
	return {vmin(box.min(), point), vmax(box.max(), point)};
}

template <class T> Box<T> enclose(const T &point, const Box<T> &box) { return enclose(box, point); }

template <class T> auto intersection(const Box<T> &lhs, const Box<T> &rhs) {
	return lhs.intersection(rhs);
}

template <class T> auto intersectionOrEmpty(const Box<T> &lhs, const Box<T> &rhs) {
	return lhs.intersectionOrEmpty(rhs);
}

template <class T> bool overlaps(const Box<T> &lhs, const Box<T> &rhs) {
	auto result = lhs.intersection(rhs);
	return result && !result->empty();
}

template <class T> bool overlapsNotEmpty(const Box<T> &lhs, const Box<T> &rhs) {
	return !lhs.empty() && !rhs.empty() && overlaps(lhs, rhs);
}

template <class T> bool touches(const Box<T> &lhs, const Box<T> &rhs) {
	auto result = lhs.intersection(rhs);
	return result && result->empty();
}

FBox encloseTransformed(const FBox &, const Matrix4 &);

// Stored just like in OpenGL:
// Column major order, vector post multiplication
class Matrix3 {
  public:
	Matrix3() = default;
	Matrix3(const Matrix3 &) = default;
	Matrix3(const float3 &col0, const float3 &col1, const float3 &col2) {
		v[0] = col0;
		v[1] = col1;
		v[2] = col2;
	}

	static const Matrix3 identity();

	const float3 row(int n) const { return float3(v[0][n], v[1][n], v[2][n]); }

	float operator()(int row, int col) const { return v[col][row]; }
	float &operator()(int row, int col) { return v[col][row]; }

	// Access columns
	const float3 &operator[](int n) const { return v[n]; }
	float3 &operator[](int n) { return v[n]; }

	FWK_VEC_RANGE()

  private:
	float3 v[3];
};

inline bool operator==(const Matrix3 &lhs, const Matrix3 &rhs) {
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

const Matrix3 transpose(const Matrix3 &);

// Equivalent to creating the matrix with column{0,1,2} as rows
const Matrix3 transpose(const float3 &column0, const float3 &column1, const float3 &column2);
const Matrix3 inverse(const Matrix3 &);

const Matrix3 operator*(const Matrix3 &, const Matrix3 &);
const float3 operator*(const Matrix3 &, const float3 &);

const Matrix3 scaling(const float3 &);
const Matrix3 rotation(const float3 &axis, float angle);

// TODO: what's this?
inline const Matrix3 normalRotation(float angle) { return rotation(float3(0, -1, 0), angle); }

// Stored just like in OpenGL:
// Column major order, vector post multiplication
class Matrix4 {
  public:
	Matrix4() = default;
	Matrix4(const Matrix4 &) = default;
	Matrix4(const Matrix3 &mat) {
		v[0] = float4(mat[0], 0.0f);
		v[1] = float4(mat[1], 0.0f);
		v[2] = float4(mat[2], 0.0f);
		v[3] = float4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	Matrix4(const float4 &col0, const float4 &col1, const float4 &col2, const float4 &col3) {
		v[0] = col0;
		v[1] = col1;
		v[2] = col2;
		v[3] = col3;
	}
	Matrix4(CRange<float, 16> values) { // TODO: use range<float, 16>
		for(int n = 0; n < 4; n++)
			v[n] = float4(CRange<float, 4>(values.data() + n * 4, 4));
	}

	static const Matrix4 identity();

	const float4 row(int n) const { return float4(v[0][n], v[1][n], v[2][n], v[3][n]); }

	float operator()(int row, int col) const { return v[col][row]; }
	float &operator()(int row, int col) { return v[col][row]; }

	const float4 &operator[](int n) const { return v[n]; }
	float4 &operator[](int n) { return v[n]; }

	const Matrix4 operator+(const Matrix4 &) const;
	const Matrix4 operator-(const Matrix4 &) const;
	const Matrix4 operator*(float)const;

	FWK_VEC_RANGE()

  private:
	float4 v[4];
};

inline bool operator==(const Matrix4 &lhs, const Matrix4 &rhs) {
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

const Matrix4 operator*(const Matrix4 &, const Matrix4 &);
const float4 operator*(const Matrix4 &, const float4 &);

const float3 mulPoint(const Matrix4 &mat, const float3 &);
const float3 mulPointAffine(const Matrix4 &mat, const float3 &);
const float3 mulNormal(const Matrix4 &inverse_transpose, const float3 &);
const float3 mulNormalAffine(const Matrix4 &affine_mat, const float3 &);

// Equivalent to creating the matrix with column{0,1,2,3} as rows
const Matrix4 transpose(const float4 &col0, const float4 &col1, const float4 &col2,
						const float4 &col3);
const Matrix4 transpose(const Matrix4 &);
const Matrix4 inverse(const Matrix4 &);

const Matrix4 translation(const float3 &);
const Matrix4 lookAt(const float3 &eye, const float3 &target, const float3 &up);
const Matrix4 perspective(float fov, float aspect_ratio, float z_near, float z_far);
const Matrix4 ortho(float left, float right, float top, float bottom, float near, float far);

inline const Matrix4 scaling(float x, float y, float z) { return scaling(float3(x, y, z)); }
inline const Matrix4 scaling(float s) { return scaling(s, s, s); }
inline const Matrix4 translation(float x, float y, float z) { return translation(float3(x, y, z)); }

class AxisAngle {
  public:
	AxisAngle() {}
	AxisAngle(const float3 &axis, float angle) : m_axis(normalize(axis)), m_angle(angle) {}
	inline operator const Matrix3() const { return rotation(m_axis, m_angle); }

	float angle() const { return m_angle; }
	const float3 axis() const { return m_axis; }

	FWK_ORDER_BY(AxisAngle, m_axis, m_angle);

  private:
	float3 m_axis;
	float m_angle;
};

// TODO: float4 should be a member not a parent
class Quat : public float4 {
  public:
	Quat() : float4(0, 0, 0, 1) {}
	Quat(const Quat &q) : float4(q) {}
	Quat &operator=(const Quat &q) {
		float4::operator=(float4(q));
		return *this;
	}

	Quat(const float3 &xyz, float w) : float4(xyz, w) {}
	Quat(float x, float y, float z, float w) : float4(x, y, z, w) {}
	explicit Quat(const float4 &v) : float4(v) {}
	explicit Quat(const Matrix3 &);
	explicit Quat(const AxisAngle &);

	static const Quat fromYawPitchRoll(float y, float p, float r);

	const Quat operator*(float v) const { return Quat(float4::operator*(v)); }
	const Quat operator+(const Quat &rhs) const { return Quat(float4::operator+(rhs)); }
	const Quat operator-(const Quat &rhs) const { return Quat(float4::operator-(rhs)); }
	const Quat operator-() const { return Quat(float4::operator-()); }

	operator Matrix3() const;
	operator AxisAngle() const;

	const Quat operator*(const Quat &)const;

	FWK_ORDER_BY(Quat, x, y, z, w);
};

inline float dot(const Quat &lhs, const Quat &rhs) { return dot(float4(lhs), float4(rhs)); }
const Quat inverse(const Quat &);
const Quat normalize(const Quat &);
const Quat slerp(const Quat &, Quat, float t);
const Quat rotationBetween(const float3 &, const float3 &);
const Quat conjugate(const Quat &);

// in radians
float distance(const Quat &, const Quat &);

struct AffineTrans {
	explicit AffineTrans(const Matrix4 &);
	AffineTrans(const float3 &pos = float3(), const Quat &rot = Quat(),
				const float3 &scale = float3(1.0f, 1.0f, 1.0f))
		: translation(pos), scale(scale), rotation(rot) {}
	operator Matrix4() const;

	FWK_ORDER_BY(AffineTrans, translation, scale, rotation);

	float3 translation;
	float3 scale;
	Quat rotation;
};

AffineTrans operator*(const AffineTrans &, const AffineTrans &);
AffineTrans lerp(const AffineTrans &, const AffineTrans &, float t);

struct Triangle2D {
	Triangle2D() : m_points({{float2(), float2(), float2()}}) {}
	Triangle2D(const float2 &a, const float2 &b, const float2 &c) : m_points({{a, b, c}}) {}

	const float2 &operator[](int idx) const { return m_points[idx]; }
	float2 center() const { return (m_points[0] + m_points[1] + m_points[2]) / 3.0f; }

  private:
	array<float2, 3> m_points;
};

class Triangle {
  public:
	Triangle(const float3 &a, const float3 &b, const float3 &c);
	Triangle() : Triangle(float3(), float3(), float3()) {}
	using Edge = pair<float3, float3>;

	float3 operator[](int idx) const {
		DASSERT(idx >= 0 && idx < 3);
		return idx == 0 ? a() : idx == 1 ? b() : c();
	}

	bool isValid() const {
		FATAL("TODO: write me");
		return false;
	}

	float3 a() const { return m_point; }
	float3 b() const { return m_point + m_edge[0]; }
	float3 c() const { return m_point + m_edge[1]; }
	float3 center() const { return m_point + (m_edge[0] + m_edge[1]) * (1.0f / 3.0f); }
	float3 cross() const { return m_normal * m_length; }

	Triangle operator*(float scale) const {
		return Triangle(a() * scale, b() * scale, c() * scale);
	}
	Triangle operator*(const float3 &scale) const {
		return Triangle(a() * scale, b() * scale, c() * scale);
	}
	Triangle inverse() const { return Triangle(c(), b(), a()); }

	Triangle2D xz() const { return Triangle2D(a().xz(), b().xz(), c().xz()); }

	float3 barycentric(const float3 &point) const;
	vector<float3> pickPoints(float density) const;

	float3 edge1() const { return m_edge[0]; }
	float3 edge2() const { return m_edge[1]; }
	float3 normal() const { return m_normal; }
	float surfaceArea() const { return m_length * 0.5f; }
	array<float3, 3> verts() const { return {{a(), b(), c()}}; }
	array<Edge, 3> edges() const { return {{Edge(a(), b()), Edge(b(), c()), Edge(c(), a())}}; }
	array<float, 3> angles() const;

  private:
	float3 m_point;
	float3 m_edge[2];
	float3 m_normal;
	float m_length;
};

Triangle operator*(const Matrix4 &, const Triangle &);

float distance(const Triangle &, const Triangle &);
float distance(const Triangle &, const float3 &);

bool areIntersecting(const Triangle &, const Triangle &);
bool areIntersecting(const Triangle2D &, const Triangle2D &);
float distance(const Triangle2D &, const float2 &point);

// dot(plane.normal(), pointOnPlane) == plane.distance();
class Plane {
  public:
	Plane() : m_nrm(0, 0, 1), m_dist(0) {}
	Plane(const float3 &normal, float distance) : m_nrm(normal), m_dist(distance) {}
	Plane(const float3 &normal, const float3 &point) : m_nrm(normal), m_dist(dot(point, normal)) {}

	// TODO: should triangle be CW or CCW?
	explicit Plane(const Triangle &);
	Plane(const float3 &a, const float3 &b, const float3 &c) : Plane(Triangle(a, b, c)) {}

	const float3 &normal() const { return m_nrm; }

	// distance from if dir is normalized then it is a distance
	// from plane to point (0, 0, 0)
	float distance() const { return m_dist; }

	enum SideTestResult {
		all_negative = -1,
		both_sides = 0,
		all_positive = 1,
	};
	SideTestResult sideTest(CRange<float3> verts) const;
	Plane operator-() const { return Plane(-m_nrm, -m_dist); }

	FWK_ORDER_BY(Plane, m_nrm, m_dist);

  private:
	float3 m_nrm;
	float m_dist;
};

class Tetrahedron {
  public:
	using FaceIndices = array<int, 3>;
	using Edge = pair<float3, float3>;

	Tetrahedron(const float3 &p1, const float3 &p2, const float3 &p3, const float3 &p4);
	Tetrahedron(CRange<float3, 4> points)
		: Tetrahedron(points[0], points[1], points[2], points[3]) {}
	Tetrahedron() : Tetrahedron(float3(), float3(), float3(), float3()) {}

	static array<FaceIndices, 4> faces();

	array<Plane, 4> planes() const;
	array<Triangle, 4> tris() const;
	array<Edge, 6> edges() const;

	float volume() const;
	float surfaceArea() const;
	float inscribedSphereRadius() const;

	bool isIntersecting(const Triangle &) const;
	bool isInside(const float3 &vec) const;
	bool isValid() const;

	const float3 &operator[](int idx) const { return m_verts[idx]; }
	const float3 &corner(int idx) const { return m_verts[idx]; }
	const auto &verts() const { return m_verts; }
	float3 center() const { return (m_verts[0] + m_verts[1] + m_verts[2] + m_verts[3]) * 0.25f; }

	FWK_ORDER_BY(Tetrahedron, m_verts);

  private:
	array<float3, 4> m_verts;
};

Tetrahedron fixVolume(const Tetrahedron &);

inline auto verts(const Tetrahedron &tet) { return tet.verts(); }

// Planes are pointing outwards (like the faces)
inline auto planes(const Tetrahedron &tet) { return tet.planes(); }
inline auto edges(const Tetrahedron &tet) { return tet.edges(); }

inline float3 normalizedDirection(const pair<float3, float3> &edge) {
	return normalize(edge.second - edge.first);
}

// Source: http://www.geometrictools.com/Documentation/MethodOfSeparatingAxes.pdf
// TODO: planes should be pointing inwards, not outwards...
template <class Convex1, class Convex2> bool satTest(const Convex1 &a, const Convex2 &b) {
	auto a_verts = verts(a);
	auto b_verts = verts(b);

	for(const auto &plane : planes(a))
		if(plane.sideTest(b_verts) == 1)
			return false;
	for(const auto &plane : planes(b))
		if(plane.sideTest(a_verts) == 1)
			return false;

	auto a_edges = edges(a);
	auto b_edges = edges(b);
	for(const auto &edge_a : a_edges) {
		float3 nrm_a = normalize(edge_a.second - edge_a.first);
		for(const auto &edge_b : b_edges) {
			float3 nrm_b = normalize(edge_b.second - edge_b.first);
			float3 plane_nrm = normalize(cross(nrm_a, nrm_b));
			int side_a = Plane(plane_nrm, edge_a.first).sideTest(a_verts);
			if(side_a == 0)
				continue;

			int side_b = Plane(plane_nrm, edge_a.first).sideTest(b_verts);
			if(side_b == 0)
				continue;

			if(side_a * side_b < 0)
				return false;
		}
	}

	return true;
}

bool areIntersecting(const Tetrahedron &, const Tetrahedron &);

const Plane normalize(const Plane &);
const Plane operator*(const Matrix4 &, const Plane &);

// TODO: change name to distance
float dot(const Plane &, const float3 &point);

template <class Real, int N> class TRay {
  public:
	static_assert(isReal<Real>(), "Ray cannot be constructed using integral numbers as base type");
	using Scalar = Real;
	using Vector = MakeVector<Real, N>;
	using Point = Vector;

	TRay(const Vector &origin, const Vector &dir) : m_origin(origin), m_dir(dir) {
		DASSERT(!isZero(m_dir));
		DASSERT(isNormalized(m_dir));
	}

	const auto &dir() const { return m_dir; }
	const auto &origin() const { return m_origin; }
	auto invDir() const { return inv(m_dir); }
	const float3 at(float t) const { return m_origin + m_dir * t; }

	// TODO: should we allow empty rays?
	bool empty() const { return isZero(m_dir); }

	FWK_ORDER_BY(TRay, m_origin, m_dir);

  private:
	Point m_origin;
	Vector m_dir;
};

using Ray = TRay<float, 3>;

DEFINE_ENUM(SegmentIsectClass, shared_endpoints, point, segment, none);
template <class T> using SegmentIsectParam = Variant<None, T, pair<T, T>>;

template <class T, int N> struct ISegment {
	static_assert(isIntegral<T>(), "");
	static_assert(N >= 2 && N <= 3, "");

	using Vector = MakeVector<T, N>;
	using Scalar = T;
	using Point = Vector;
	enum { dim_size = N };

	ISegment() : from(), to() {}
	ISegment(const Point &a, const Point &b) : from(a), to(b) {}
	ISegment(const pair<Point, Point> &pair) : ISegment(pair.first, pair.second) {}

	ENABLE_IF_SIZE(2) explicit ISegment(T x1, T y1, T x2, T y2) : from(x1, y1), to(x2, y2) {}
	ENABLE_IF_SIZE(3)
	explicit ISegment(T x1, T y1, T z1, T x2, T y2, T z2) : from(x1, y1, z1), to(x2, y2, z2) {}

	template <class U>
	ISegment(const ISegment<U, N> &rhs) : ISegment(Point(rhs.from), Point(rhs.to)) {}

	bool empty() const { return from == to; }

	const Point &operator[](int n) const { return v[n]; }
	Point &operator[](int n) { return v[n]; }

	bool sharedEndPoints(const ISegment &rhs) const {
		return isOneOf(from, rhs.from, rhs.to) || isOneOf(to, rhs.from, rhs.to);
	}

	// Computations performed on qints; you have to be careful if values are greater than 32 bits
	ENABLE_IF_SIZE(2) SegmentIsectClass classifyIsect(const ISegment &) const;
	bool testIsect(const Box<Vector> &) const;

	FWK_VEC_RANGE()
	FWK_ORDER_BY(ISegment, from, to)

	union {
		struct {
			Point from, to;
		};
		Point v[2];
	};
};

template <class T> using ISegment2 = ISegment<T, 2>;
template <class T> using ISegment3 = ISegment<T, 3>;

template <class T, int N> struct Segment {
	static_assert(!isIntegral<T>(), "use ISegment for integer-based segments");
	static_assert(isReal<T>(), "");
	static_assert(N >= 2 && N <= 3, "");

	using Vector = MakeVector<T, N>;
	using Scalar = T;
	using Point = Vector;
	enum { dim_size = N };

	Segment() : from(), to() {}
	Segment(const Point &a, const Point &b) : from(a), to(b) {}
	Segment(const pair<Point, Point> &pair) : Segment(pair.first, pair.second) {}

	ENABLE_IF_SIZE(2) explicit Segment(T x1, T y1, T x2, T y2) : from(x1, y1), to(x2, y2) {}
	ENABLE_IF_SIZE(3)
	explicit Segment(T x1, T y1, T z1, T x2, T y2, T z2) : from(x1, y1, z1), to(x2, y2, z2) {}

	template <class U>
	explicit Segment(const Segment<U, N> &rhs) : Segment(Point(rhs.from), Point(rhs.to)) {}

	template <class U> explicit operator ISegment<U, N>() const {
		using IVector = MakeVector<U, N>;
		return {IVector(from), IVector(to)};
	}
	template <class U>
	explicit Segment(const ISegment<U, N> &iseg) : from(iseg.from), to(iseg.to) {}

	bool empty() const { return from == to; }

	Maybe<TRay<T, N>> asRay() const;

	auto length() const { return fwk::distance(from, to); }
	T lengthSq() const { return fwk::distanceSq(from, to); }

	// 0.0 -> from; 1.0 -> to
	Vector at(T param) const { return from + (to - from) * param; }
	Segment subSegment(T p1, T p2) const { return Segment(at(p1), at(p2)); }

	const Point &operator[](int n) const { return v[n]; }
	Point &operator[](int n) { return v[n]; }

	T distanceSq(const Point &) const;
	T distanceSq(const Segment &) const;

	T distance(const Point &point) const { return std::sqrt(distanceSq(point)); }
	T distance(const Segment &seg) const { return std::sqrt(distanceSq(seg)); }

	bool sharedEndPoints(const Segment &rhs) const {
		return isOneOf(from, rhs.from, rhs.to) || isOneOf(to, rhs.from, rhs.to);
	}

	using IsectParam = SegmentIsectParam<T>;
	using Isect = Variant<None, Segment, Vector>;

	Isect at(const IsectParam &) const;

	ENABLE_IF_SIZE(2) IsectParam isectParam(const Segment &) const;
	ENABLE_IF_SIZE(2) Isect isect(const Segment &) const;

	ENABLE_IF_SIZE(2) SegmentIsectClass classifyIsect(const Segment &) const;
	bool testIsect(const Box<Vector> &) const;

	IsectParam isectParam(const Box<Vector> &) const;
	Isect isect(const Box<Vector> &) const;

	T closestPointParam(const Point &) const;
	T closestPointParam(const Segment &) const;
	pair<T, T> closestPointParams(const Segment &) const;

	Vector closestPoint(const Point &pt) const;
	Vector closestPoint(const Segment &) const;
	pair<Vector, Vector> closestPoints(const Segment &rhs) const;

	ENABLE_IF_SIZE(3) Segment<T, 2> xz() const { return {from.xz(), to.xz()}; }
	ENABLE_IF_SIZE(3) Segment<T, 2> xy() const { return {from.xy(), to.xy()}; }
	ENABLE_IF_SIZE(3) Segment<T, 2> yz() const { return {from.yz(), to.yz()}; }

	FWK_VEC_RANGE()
	FWK_ORDER_BY(Segment, from, to)

	union {
		struct {
			Point from, to;
		};
		Point v[2];
	};
};

template <class T> using Segment2 = Segment<T, 2>;
template <class T> using Segment3 = Segment<T, 3>;

float distance(const Ray &ray, const float3 &point);
float distance(const Ray &, const Ray &);
float distance(const Triangle &tri, const Segment3<float> &);

float3 closestPoint(const Ray &, const float3 &point);
float3 closestPoint(const Triangle &, const float3 &point);
float3 closestPoint(const Plane &, const float3 &point);

pair<float3, float3> closestPoints(const Ray &, const Ray &);

// returns infinity if doesn't intersect
pair<float, float> intersectionRange(const Ray &, const Box<float3> &box);
pair<float, float> intersectionRange(const Segment3<float> &, const Box<float3> &box);

pair<Segment3<float>, bool> intersectionSegment(const Triangle &, const Triangle &);

inline float intersection(const Ray &ray, const Box<float3> &box) {
	return intersectionRange(ray, box).first;
}

inline float intersection(const Segment3<float> &segment, const Box<float3> &box) {
	return intersectionRange(segment, box).first;
}

float intersection(const Ray &, const Triangle &);
float intersection(const Segment3<float> &, const Triangle &);

Segment3<float> operator*(const Matrix4 &, const Segment3<float> &);

float intersection(const Segment3<float> &, const Plane &);
float intersection(const Ray &, const Plane &);

inline float intersection(const Plane &plane, const Segment3<float> &segment) {
	return intersection(segment, plane);
}
inline float intersection(const Plane &plane, const Ray &ray) { return intersection(ray, plane); }
inline float intersection(const Triangle &tri, const Segment3<float> &segment) {
	return intersection(segment, tri);
}

Maybe<Ray> intersection(const Plane &, const Plane &);

class Projection {
  public:
	Projection(const float3 &origin, const float3 &vec_x, const float3 &vec_y);
	// X: edge1 Y: -normal
	Projection(const Triangle &tri);

	float3 project(const float3 &) const;
	float3 unproject(const float3 &) const;

	float3 projectVector(const float3 &) const;
	float3 unprojectVector(const float3 &) const;

	Triangle project(const Triangle &) const;
	Segment3<float> project(const Segment3<float> &) const;

	// TODO: maybe those operators aren't such a good idea?
	template <class T> auto operator*(const T &obj) const { return project(obj); }
	template <class T> auto operator/(const T &obj) const { return unproject(obj); }

  private:
	Matrix3 m_base, m_ibase;
	float3 m_origin;
};

// Basically, a set of planes
class Frustum {
  public:
	enum {
		id_left,
		id_right,
		id_up,
		id_down,
		//	id_front,
		//	id_back

		planes_count,
	};

	Frustum() = default;
	Frustum(const Matrix4 &view_projection);
	Frustum(CRange<Plane, planes_count>);

	bool isIntersecting(const float3 &point) const;
	bool isIntersecting(const FBox &box) const;

	// TODO: clang crashes on this (in full opt with -DNDEBUG)
	bool isIntersecting(CRange<float3> points) const;

	const Plane &operator[](int idx) const { return m_planes[idx]; }
	Plane &operator[](int idx) { return m_planes[idx]; }
	int size() const { return planes_count; }

  private:
	std::array<Plane, planes_count> m_planes;
};

const Frustum operator*(const Matrix4 &, const Frustum &);

class Cylinder {
  public:
	Cylinder(const float3 &pos, float radius, float height)
		: m_pos(pos), m_radius(radius), m_height(height) {}

	const float3 &pos() const { return m_pos; }
	float radius() const { return m_radius; }
	float height() const { return m_height; }

	FBox enclosingBox() const {
		return FBox({-m_radius, 0, -m_radius}, {m_radius, m_height, m_radius}) + m_pos;
	}
	Cylinder operator+(const float3 &offset) const {
		return Cylinder(m_pos + offset, m_radius, m_height);
	}

	FWK_ORDER_BY(Cylinder, m_pos, m_radius, m_height);

  private:
	float3 m_pos;
	float m_radius;
	float m_height;
};

float distance(const Cylinder &, const float3 &);
bool areIntersecting(const Cylinder &, const Cylinder &);
bool areIntersecting(const FBox &, const Cylinder &);

array<Plane, 6> planes(const FBox &);
array<pair<float3, float3>, 12> edges(const FBox &);
array<float3, 8> verts(const FBox &);

using RandomSeed = unsigned long;

class Random {
  public:
	Random(RandomSeed = 123);

	RandomSeed operator()();

	int uniform(int min, int max);
	int uniform(int up_to) { return uniform(0, up_to - 1); }
	float uniform(float min, float max);
	double uniform(double min, double max);

	template <class T, EnableIfVector<T>...> T sampleBox(const T &min, const T &max) {
		T out;
		for(int n = 0; n < T::vector_size; n++)
			out[n] = uniform(min[n], max[n]);
		return out;
	}
	template <class T, EnableIfVector<T>...> T sampleUnitHemisphere() {
		auto point = sampleUnitSphere<T>();
		while(isZero(point))
			point = sampleUnitSphere<T>();
		return normalize(point);
	}

	template <class T, EnableIfVector<T>...> T sampleUnitSphere() {
		using Scalar = typename T::Scalar;
		T one;
		for(int n = 0; n < T::vector_size; n++)
			one[n] = Scalar(1);
		auto out = sampleBox(-one, one);
		while(lengthSq(out) > Scalar(1))
			out = sampleBox(-one, one);
		return out;
	}

	Quat uniformRotation();
	Quat uniformRotation(float3 axis);

  private:
	std::default_random_engine m_engine;
};

template <class Value = int> struct Hash {
	// Source: Blender
	static Value hashCombine(Value hash_a, Value hash_b) {
		return hash_a ^ (hash_b + 0x9e3779b9 + (hash_a << 6) + (hash_a >> 2));
	}

	template <class T, EnableIfScalar<T>...> static Value hash(T scalar) {
		return std::hash<T>()(scalar);
	}

	template <class T, EnableIf<IsRange<T>::value && !isTied<T>(), NotARange>...>
	static Value hash(const T &trange) {
		auto range = makeConstRange(trange);
		if(range.empty())
			return 0;
		auto out = hash(range[0]);
		for(int n = 1; n < range.size(); n++)
			out = hashCombine(out, hash(range[n]));
		return out;
	}

	template <class T>
	static typename std::enable_if<std::is_enum<T>::value, Value>::type hash(const T val) {
		return hash((int)val);
	}
	template <class T1, class T2> static Value hash(const std::pair<T1, T2> &pair) {
		return hashCombine(hash(pair.first), hash(pair.second));
	}
	template <class... Types> static Value hash(const std::tuple<Types...> &tuple) {
		return hashTuple<0>(tuple);
	}
	template <class T, EnableIfTied<T>...> static Value hash(const T &object) {
		return hashTuple<0>(object.tied());
	}

	template <int N, class... Types>
	static auto hashTuple(const std::tuple<Types...> &tuple) ->
		typename std::enable_if<N + 1 == sizeof...(Types), Value>::type {
		return hash(std::get<N>(tuple));
	}

	template <int N, class... Types>
	static auto hashTuple(const std::tuple<Types...> &tuple) ->
		typename std::enable_if<N + 1 < sizeof...(Types), Value>::type {
		return hashCombine(hash(std::get<N>(tuple)), hashTuple<N + 1>(tuple));
	}

	template <class T> auto operator()(const T &value) const { return hash(value); }
};

template <class T> int hash(const T &value) { return Hash<int>()(value); }

static_assert(sizeof(Matrix3) == sizeof(float3) * 3, "Wrong size of Matrix3 class");
static_assert(sizeof(Matrix4) == sizeof(float4) * 4, "Wrong size of Matrix4 class");
}

SERIALIZE_AS_POD(short2)
SERIALIZE_AS_POD(int2)
SERIALIZE_AS_POD(int3)
SERIALIZE_AS_POD(int4)
SERIALIZE_AS_POD(llint2)
SERIALIZE_AS_POD(qint2)

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
SERIALIZE_AS_POD(Ray)

SERIALIZE_AS_POD(Segment2<float>)
SERIALIZE_AS_POD(Segment3<float>)
SERIALIZE_AS_POD(Segment2<double>)
SERIALIZE_AS_POD(Segment3<double>)

SERIALIZE_AS_POD(ISegment2<int>)
SERIALIZE_AS_POD(ISegment2<llint>)
SERIALIZE_AS_POD(ISegment2<qint>)

#undef ENABLE_IF_SIZE

#endif
