/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#ifndef FWK_MATH_H
#define FWK_MATH_H

#include "fwk_base.h"
#include <cmath>
#include <limits>
#include <random>

namespace fwk {

#define VEC_RANGE()                                                                                \
	auto begin() { return v; }                                                                     \
	auto end() { return v + arraySize(v); }                                                        \
	auto begin() const { return v; }                                                               \
	auto end() const { return v + arraySize(v); }                                                  \
	auto data() { return v; }                                                                      \
	auto data() const { return v; }                                                                \
	constexpr auto size() const { return arraySize(v); }

// TODO: make sure that all classes / structures here have proper default constructor (for
// example AxisAngle requires fixing)

// TODO: remove epsilons?
namespace fconstant {
	static constexpr const float pi = 3.14159265358979f;
	static constexpr const float e = 2.71828182845905f;
	static constexpr const float epsilon = 0.0001f;
	static constexpr const float isect_epsilon = 0.000000001f;
	static const float inf = std::numeric_limits<float>::infinity();
}

namespace dconstant {
	static constexpr const double pi = 3.14159265358979;
	static constexpr const double e = 2.71828182845905;
	static constexpr const double epsilon = 0.0001;
	static constexpr const double isect_epsilon = 0.000000001;
	static const double inf = std::numeric_limits<double>::infinity();
}

template <class Real> struct constant {
	static_assert(std::is_floating_point<Real>::value, "");

	static const constexpr Real one = 1.0;
	static const constexpr Real pi = 3.14159265358979;
	static const constexpr Real e = 2.71828182845905;
	static const constexpr Real epsilon = 0.0001;
	static const constexpr Real isect_epsilon = 0.000000001;
	static Real inf() { return std::numeric_limits<Real>::infinity(); }
};

struct short2 {
	using Scalar = short;
	enum { vector_size = 2 };

	short2(short x, short y) : x(x), y(y) {}
	short2() : x(0), y(0) {}

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
	VEC_RANGE()

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

	int2(short2 rhs) : x(rhs.x), y(rhs.y) {}
	int2(int x, int y) : x(x), y(y) {}
	int2() : x(0), y(0) {}

	operator short2() const { return short2(x, y); }

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
	VEC_RANGE()

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

	int3(int x, int y, int z) : x(x), y(y), z(z) {}
	int3() : x(0), y(0), z(0) {}

	int3 operator+(const int3 &rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
	int3 operator-(const int3 &rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
	int3 operator*(const int3 &rhs) const { return {x * rhs.x, y * rhs.y, z * rhs.z}; }
	int3 operator*(int s) const { return {x * s, y * s, z * s}; }
	int3 operator/(int s) const { return {x / s, y / s, z / s}; }
	int3 operator%(int s) const { return {x % s, y % s, z % s}; }

	int2 xy() const { return {x, y}; }
	int2 xz() const { return {x, z}; }
	int2 yz() const { return {y, z}; }

	int &operator[](int idx) { return v[idx]; }
	const int &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(int3, x, y, z)
	VEC_RANGE()

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

	int4(int x, int y, int z, int w) : x(x), y(y), z(z), w(w) {}
	int4() : x(0), y(0), z(0), w(0) {}

	int4 operator+(const int4 rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; }
	int4 operator-(const int4 rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; }
	int4 operator*(int s) const { return {x * s, y * s, z * s, w * s}; }
	int4 operator/(int s) const { return {x / s, y / s, z / s, w / s}; }

	int &operator[](int idx) { return v[idx]; }
	const int &operator[](int idx) const { return v[idx]; }

	FWK_ORDER_BY(int4, x, y, z, w)
	VEC_RANGE()

	union {
		struct {
			int x, y, z, w;
		};
		int v[4];
	};
};

struct float2 {
	using Scalar = float;
	enum { vector_size = 2 };

	float2(float x, float y) : x(x), y(y) {}
	float2() : x(0.0f), y(0.0f) {}

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
	VEC_RANGE()

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

	float3(float x, float y, float z) : x(x), y(y), z(z) {}
	float3(const float2 &xy, float z) : x(xy.x), y(xy.y), z(z) {}
	float3() : x(0.0f), y(0.0f), z(0.0f) {}

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
	VEC_RANGE()

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
	float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	float4(const float3 &xyz, float w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
	float4(const float2 &xy, float z, float w) : x(xy.x), y(xy.y), z(z), w(w) {}
	float4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

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
	VEC_RANGE()

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

	double2(double x, double y) : x(x), y(y) {}
	double2() : x(0.0f), y(0.0f) {}

	explicit double2(const int2 &vec) : x(vec.x), y(vec.y) {}
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
	VEC_RANGE()

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

	double3(double x, double y, double z) : x(x), y(y), z(z) {}
	double3(const double2 &xy, double z) : x(xy.x), y(xy.y), z(z) {}
	double3() : x(0.0f), y(0.0f), z(0.0f) {}

	explicit double3(const int3 &vec) : x(vec.x), y(vec.y), z(vec.z) {}
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
	VEC_RANGE()

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
	double4(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {}
	double4(const double3 &xyz, double w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
	double4(const double2 &xy, double z, double w) : x(xy.x), y(xy.y), z(z), w(w) {}
	double4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

	explicit double4(const int4 &vec) : x(vec.x), y(vec.y), z(vec.z), w(vec.w) {}
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
	VEC_RANGE()

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
struct NotAVector;
template <int N> struct NotAVectorN;

namespace detail {

	template <class T> using IsReal = std::is_floating_point<T>;
	template <class T> using IsIntegral = std::is_integral<T>;
	template <class T> struct IsScalar {
		enum { value = std::is_floating_point<T>::value || std::is_integral<T>::value };
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
		using RetError = typename std::conditional<N == 0, NotAVector, NotAVectorN<N>>::type;
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

		using IntegralRet = typename std::conditional<Get::is_integral, Ret, RetError>::type;
		using RealRet = typename std::conditional<Get::is_real, Ret, RetError>::type;
	};

	template <class T, int N = 0> struct IsVector {
		enum { value = VectorInfo<T, N>::size > 0 };
	};

	// TODO: better name? also differentiate from fwk::vector
	template <class T, int N> struct MakeVector { using type = NotAVector; };
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

template <class Arg, class T>
using EnableIfScalar = typename std::conditional<isScalar<T>(), detail::ValidType,
												 NotAScalar>::type::template Arg<Arg>;
template <class Arg, class T>
using EnableIfReal = typename std::conditional<std::is_floating_point<T>::value, detail::ValidType,
											   NotAReal>::type::template Arg<Arg>;
template <class Arg, class T>
using EnableIfIntegral = typename std::conditional<std::is_integral<T>::value, detail::ValidType,
												   NotAIntegral>::type::template Arg<Arg>;

template <class Arg, class T>
using EnableIfMathObject = typename std::conditional<isMathObject<T>(), detail::ValidType,
													 NotAMathObject>::type::template Arg<Arg>;
template <class T> using AsMathObject = EnableIfMathObject<T, T>;

template <class Arg, class T, int N = 0>
using EnableIfVector = typename detail::VectorInfo<T, N>::Ret::template Arg<Arg>;
template <class Arg, class T, int N = 0>
using EnableIfRealVector = typename detail::VectorInfo<T, N>::RealRet::template Arg<Arg>;

template <class T, int N = 0> using AsVectorScalar = typename detail::VectorInfo<T, N>::Ret::Scalar;
template <class T, int N = 0> using AsVector = typename detail::VectorInfo<T, N>::Ret::Vector;

template <class T, int N = 0>
using AsRealVector = typename detail::VectorInfo<T, N>::RealRet::Vector;
template <class T, int N = 0>
using AsIntegralVector = typename detail::VectorInfo<T, N>::IntegralRet::Vector;

template <class T, int N>
using MakeVector = typename detail::MakeVector<typename detail::GetScalar<T>::type, N>::type;

template <class T> using Vector2 = MakeVector<T, 2>;
template <class T> using Vector3 = MakeVector<T, 3>;
template <class T> using Vector4 = MakeVector<T, 4>;

template <class T> struct ToReal { using type = double; };
template <> struct ToReal<float> { using type = float; };

template <class T, class T1> const AsMathObject<T> &operator+=(T &a, const T1 &b) {
	return a = a + b;
}
template <class T, class T1> const AsMathObject<T> &operator-=(T &a, const T1 &b) {
	return a = a - b;
}
template <class T, class T1> const AsMathObject<T> &operator*=(T &a, const T1 &b) {
	return a = a * b;
}
template <class T, class T1> const AsMathObject<T> &operator/=(T &a, const T1 &b) {
	return a = a / b;
}

template <class T> EnableIfMathObject<bool, T> operator>(const T &a, const T &b) { return b < a; }

template <class T> EnableIfMathObject<bool, T> operator>=(const T &a, const T &b) {
	return !(a < b);
}

template <class T> EnableIfMathObject<bool, T> operator<=(const T &a, const T &b) {
	return !(b < a);
}

template <class T> EnableIfScalar<T, T> clamp(const T &obj, const T &tmin, const T &tmax) {
	return min(tmax, max(tmin, obj));
}

template <class Real> EnableIfReal<Real, Real> degToRad(Real v) {
	return v * (Real(2.0) * constant<Real>::pi / Real(360.0));
}

template <class Real> EnableIfReal<Real, Real> radToDeg(Real v) {
	return v * (Real(360.0) / (Real(2.0) * constant<Real>::pi));
}

// Return angle in range (0; 2 * PI)
float normalizeAngle(float radians);

template <class Obj, class Scalar> inline Obj lerp(const Obj &a, const Obj &b, const Scalar &x) {
	return (b - a) * x + a;
}

template <class T> AsVector<T> operator*(typename T::Scalar s, const T &v) { return v * s; }

template <class T> AsVector<T, 2> vmin(const T &lhs, const T &rhs) {
	return T(min(lhs[0], rhs[0]), min(lhs[1], rhs[1]));
}

template <class T> AsVector<T, 3> vmin(const T &lhs, const T &rhs) {
	return T(min(lhs[0], rhs[0]), min(lhs[1], rhs[1]), min(lhs[2], rhs[2]));
}

template <class T> AsVector<T, 4> vmin(const T &lhs, const T &rhs) {
	return T(min(lhs[0], rhs[0]), min(lhs[1], rhs[1]), min(lhs[2], rhs[2]), min(lhs[3], rhs[3]));
}

template <class T> AsVector<T, 2> vmax(const T &lhs, const T &rhs) {
	return T(max(lhs[0], rhs[0]), max(lhs[1], rhs[1]));
}

template <class T> AsVector<T, 3> vmax(const T &lhs, const T &rhs) {
	return T(max(lhs[0], rhs[0]), max(lhs[1], rhs[1]), max(lhs[2], rhs[2]));
}

template <class T> AsVector<T, 4> vmax(const T &lhs, const T &rhs) {
	return T(max(lhs[0], rhs[0]), max(lhs[1], rhs[1]), max(lhs[2], rhs[2]), max(lhs[3], rhs[3]));
}

template <class T> AsVector<T> vclamp(const T &vec, const T &tmin, const T &tmax) {
	return vmin(tmax, vmax(tmin, vec));
}

template <class T> EnableIfVector<pair<T, T>, T> vecMinMax(CRange<T> range) {
	if(range.empty())
		return make_pair(T(), T());
	T tmin = range[0], tmax = range[0];
	for(int n = 1; n < range.size(); n++) {
		tmin = vmin(tmin, range[n]);
		tmax = vmax(tmax, range[n]);
	}
	return make_pair(tmin, tmax);
}

template <class TRange, class T = typename ContainerBaseType<TRange>::type>
EnableIfVector<pair<T, T>, T> vecMinMax(const TRange &range) {
	return vecMinMax(makeRange(range));
}

template <class T> AsVectorScalar<T, 2> dot(const T &lhs, const T &rhs) {
	return lhs.x * rhs.x + lhs.y * rhs.y;
}

template <class T> AsVectorScalar<T, 3> dot(const T &lhs, const T &rhs) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

template <class T> AsVectorScalar<T, 4> dot(const T &lhs, const T &rhs) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

template <class T>
EnableIfVector<decltype(std::sqrt(AsVectorScalar<T>())), T> length(const T &vec) {
	return std::sqrt(dot(vec, vec));
}

template <class T> AsVectorScalar<T> lengthSq(const T &vec) { return dot(vec, vec); }

template <class T>
EnableIfVector<decltype(std::sqrt(AsVectorScalar<T>())), T> distance(const T &lhs, const T &rhs) {
	return length(lhs - rhs);
}

template <class T> AsVectorScalar<T> distanceSq(const T &lhs, const T &rhs) {
	return lengthSq(lhs - rhs);
}

template <class T> AsRealVector<T> normalize(const T &v) { return v / length(v); }

template <class T> AsVector<T, 2> vabs(const T &v) { return {std::abs(v.x), std::abs(v.y)}; }

template <class T> AsVector<T, 3> vabs(const T &v) {
	return {std::abs(v.x), std::abs(v.y), std::abs(v.z)};
}

template <class T> AsVector<T, 4> vabs(const T &v) {
	return T(std::abs(v.x), std::abs(v.y), std::abs(v.z), std::abs(v.w));
}

template <class T> EnableIfVector<Vector3<T>, T, 2> asXZ(const T &v) { return {v[0], 0, v[1]}; }
template <class T> EnableIfVector<Vector3<T>, T, 2> asXY(const T &v) { return {v[0], v[1], 0}; }
template <class T> Vector3<T> asXZY(const T &xz, AsVectorScalar<T, 2> y) {
	return {xz[0], y, xz[1]};
}

template <class T> AsVector<T, 3> asXZY(const T &v) { return {v[0], v[2], v[1]}; }

template <class T> AsRealVector<T, 2> inv(const T &v) {
	const auto one = constant<typename T::Scalar>::one;
	return {one / v.x, one / v.y};
}
template <class T> AsRealVector<T, 3> inv(const T &v) {
	const auto one = constant<typename T::Scalar>::one;
	return {one / v.x, one / v.y, one / v.z};
}
template <class T> AsRealVector<T, 4> inv(const T &v) {
	const auto one = constant<typename T::Scalar>::one;
	return {one / v.x, one / v.y, one / v.z, one / v.w};
}

template <class T> AsVectorScalar<T, 2> cross(const T &a, const T &b) {
	return a.x * b.y - a.y * b.x;
}

template <class T> AsVector<T, 3> cross(const T &a, const T &b) {
	return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

template <class T> AsVector<T, 2> perpendicular(const T &v) { return {v.y, -v.x}; }

float vectorToAngle(const float2 &normalized_vector);
double vectorToAngle(const double2 &normalized_vector);

float2 angleToVector(float radians);
double2 angleToVector(double radians);

float2 rotateVector(const float2 &vec, float radians);
float3 rotateVector(const float3 &pos, const float3 &axis, float angle);

float angleBetween(const float2 &vec1, const float2 &vec2);
double angleBetween(const double2 &vec1, const double2 &vec2);

float angleBetween(const float2 &prev, const float2 &cur, const float2 &next);
double angleBetween(const double2 &prev, const double2 &cur, const double2 &next);

// TODO: remove it?
template <class T>
EnableIfRealVector<bool, T>
areClose(const T &a, const T &b,
		 typename T::Scalar epsilon_sq = constant<typename T::Scalar>::epsilon) {
	return distanceSq(a, b) < epsilon_sq;
}

// TODO: we can't really check it properly for floating-point's...
template <class T> EnableIfRealVector<bool, T> isNormalized(const T &vec) {
	using Real = typename T::Scalar;
	auto length_sq = lengthSq(vec);
	return length_sq >= constant<Real>::one - constant<Real>::epsilon &&
		   length_sq <= constant<Real>::one + constant<Real>::epsilon;
}

template <class T> EnableIfVector<bool, T> isZero(const T &vec) { return vec == T(); }

float frand();
float angleDistance(float a, float b);
float blendAngles(float initial, float target, float step);

// Returns angle in range <0, 2 * PI)
float normalizeAngle(float angle);

bool isnan(float);
bool isnan(double);

template <class T> EnableIfRealVector<bool, T> isnan(const T &v) {
	return anyOf(v, [](auto s) { return isnan(s); });
}

class Matrix3;
class Matrix4;

template <class TVec2> struct Rect {
	using Vec2 = TVec2;
	using Scalar = typename TVec2::Scalar;

	template <class TTVec2>
	explicit Rect(const Rect<TTVec2> &other) : min(other.min), max(other.max) {}
	explicit Rect(Vec2 size) : min(0, 0), max(size) {}
	Rect(Vec2 min, Vec2 max) : min(min), max(max) {}
	Rect(const pair<Vec2, Vec2> &min_max) : min(min_max.first), max(min_max.second) {}
	Rect(Scalar minX, Scalar minY, Scalar maxX, Scalar maxY) : min(minX, minY), max(maxX, maxY) {}
	Rect(CRange<Vec2>);
	Rect() = default;

	Scalar width() const { return max.x - min.x; }
	Scalar height() const { return max.y - min.y; }
	void setWidth(Scalar width) { max.x = min.x + width; }
	void setHeight(Scalar height) { max.y = min.y + height; }
	void setPos(Vec2 pos) {
		max += pos - min;
		min = pos;
	}

	Vec2 size() const { return max - min; }
	Vec2 center() const { return (max + min) / Scalar(2); }
	Scalar surfaceArea() const { return (max.x - min.x) * (max.y - min.y); }

	Rect operator+(const Vec2 &offset) const { return Rect(min + offset, max + offset); }
	Rect operator-(const Vec2 &offset) const { return Rect(min - offset, max - offset); }
	Rect operator*(const Vec2 &scale) const { return Rect(min * scale, max * scale); }
	Rect operator*(Scalar scale) const { return Rect(min * scale, max * scale); }

	// TODO: remove it
	Rect operator+(const Rect &rhs) { return Rect(vmin(min, rhs.min), vmax(max, rhs.max)); }

	void include(const Vec2 &point) {
		min = vmin(min, point);
		max = vmax(max, point);
	}

	// Returns corners in clockwise order
	// TODO: CW or CCW depends on handeness...
	array<Vec2, 4> corners() const { return {{min, Vec2(min.x, max.y), max, Vec2(max.x, min.y)}}; }

	bool empty() const { return max.x <= min.x || max.y <= min.y; }

	// TODO: remove or specialize for int/float
	bool isInside(const Vec2 &point) const {
		return point.x >= min.x && point.x < max.x && point.y >= min.y && point.y < max.y;
	}

	bool operator==(const Rect &rhs) const { return min == rhs.min && max == rhs.max; }

	Vec2 min, max;
};

// TODO: better name: enlarge
template <class TVec2> const Rect<TVec2> sum(const Rect<TVec2> &a, const Rect<TVec2> &b) {
	return Rect<TVec2>(vmin(a.min, b.min), vmax(a.max, b.max));
}

template <class TVec2> const Rect<TVec2> intersection(const Rect<TVec2> &a, const Rect<TVec2> &b) {
	return Rect<TVec2>(vmax(a.min, b.min), vmin(a.max, b.max));
}

template <class Type2>
inline const Rect<Type2> inset(Rect<Type2> rect, const Type2 &tl, const Type2 &br) {
	return Rect<Type2>(rect.min + tl, rect.max - br);
}

template <class Type2> inline const Rect<Type2> inset(Rect<Type2> rect, const Type2 &inset) {
	return Rect<Type2>(rect.min + inset, rect.max - inset);
}

template <class TVec> Rect<TVec> enlarge(const Rect<TVec> &rect, typename TVec::Scalar offset) {
	return Rect<TVec>(rect.min - TVec3(offset, offset, offset),
					  rect.max + TVec3(offset, offset, offset));
}

template <class TVec> Rect<TVec> enlarge(const Rect<TVec> &rect, const TVec &offset) {
	return Rect<TVec>(rect.min - offset, rect.max + offset);
}

bool areOverlapping(const Rect<float2> &a, const Rect<float2> &b);
bool areOverlapping(const Rect<int2> &a, const Rect<int2> &b);

bool areAdjacent(const Rect<int2> &, const Rect<int2> &);
float distanceSq(const Rect<float2> &, const Rect<float2> &);
const Rect<int2> enclosingIRect(const Rect<float2> &);

// Invariant: max >= min
// TODO: enforce invariant; do the same for other classes
template <class TVec3> struct Box {
	using Vec3 = TVec3;
	using Vec2 = decltype(TVec3().xz());
	using Scalar = typename TVec3::Scalar;

	template <class TTVec3>
	explicit Box(const Box<TTVec3> &other) : min(other.min), max(other.max) {}
	explicit Box(Vec3 size) : min(0, 0, 0), max(size) {}
	Box(Vec3 min, Vec3 max) : min(min), max(max) {}
	Box(const pair<Vec3, Vec3> &min_max) : min(min_max.first), max(min_max.second) {}
	Box(Scalar minX, Scalar minY, Scalar minZ, Scalar maxX, Scalar maxY, Scalar maxZ)
		: min(minX, minY, minZ), max(maxX, maxY, maxZ) {}
	Box(CRange<Vec3>);
	Box() = default;

	Scalar width() const { return max.x - min.x; }
	Scalar height() const { return max.y - min.y; }
	Scalar depth() const { return max.z - min.z; }
	Vec3 size() const { return max - min; }
	Vec3 center() const { return (max + min) / Scalar(2); }

	Box operator+(const Vec3 &offset) const { return Box(min + offset, max + offset); }
	Box operator-(const Vec3 &offset) const { return Box(min - offset, max - offset); }
	Box operator*(const Vec3 &scale) const { return Box(min * scale, max * scale); }
	Box operator*(Scalar scale) const { return Box(min * scale, max * scale); }

	// TODO: what about bounding boxes enclosing single point?
	bool empty() const { return max.x <= min.x || max.y <= min.y || max.z <= min.z; }

	// TODO: remove or specialize for int/float
	bool isInside(const Vec3 &point) const {
		return point.x >= min.x && point.x < max.x && point.y >= min.y && point.y < max.y &&
			   point.z >= min.z && point.z < max.z;
	}

	array<Vec3, 8> corners() const {
		array<Vec3, 8> out;
		for(int n = 0; n < 8; n++) {
			out[n].x = (n & 4 ? min : max).x;
			out[n].y = (n & 2 ? min : max).y;
			out[n].z = (n & 1 ? min : max).z;
		}
		return out;
	}

	const Rect<Vec2> xz() const { return Rect<Vec2>(min.xz(), max.xz()); }
	const Rect<Vec2> xy() const { return Rect<Vec2>(min.xy(), max.xy()); }
	const Rect<Vec2> yz() const { return Rect<Vec2>(min.yz(), max.yz()); }

	bool operator==(const Box &rhs) const { return min == rhs.min && max == rhs.max; }

	// TODO: these should be hidden, because we don't want to allow
	// mins which are greater than maxes.  If user wants to store invalid value
	// like this, then he should simply store it in a pair ofr vec3's.
	Vec3 min, max;
};

template <class TVec3> Box<TVec3> include(const Box<TVec3> &box, const TVec3 &point) {
	return Box<TVec3>(vmin(box.min, point), vmax(box.max, point));
}

template <class TVec3> Box<TVec3> enlarge(const Box<TVec3> &box, typename TVec3::Scalar offset) {
	return Box<TVec3>(box.min - TVec3(offset, offset, offset),
					  box.max + TVec3(offset, offset, offset));
}

template <class TVec3> Box<TVec3> enlarge(const Box<TVec3> &box, const TVec3 &offset) {
	return Box<TVec3>(box.min - offset, box.max + offset);
}

template <class TVec3> const Box<TVec3> sum(const Box<TVec3> &a, const Box<TVec3> &b) {
	return Box<TVec3>(vmin(a.min, b.min), vmax(a.max, b.max));
}

template <class TVec3> const Box<TVec3> intersection(const Box<TVec3> &a, const Box<TVec3> &b) {
	return Box<TVec3>(vmax(a.min, b.min), vmin(a.max, b.max));
}

const Box<int3> enclosingIBox(const Box<float3> &);

bool areOverlapping(const Box<float3> &a, const Box<float3> &b);
bool areOverlapping(const Box<int3> &a, const Box<int3> &b);
float distance(const Box<float3> &a, const Box<float3> &b);

typedef Rect<int2> IRect;
typedef Rect<float2> FRect;
typedef Box<int3> IBox;
typedef Box<float3> FBox;

const FBox operator*(const Matrix4 &, const FBox &);

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

	VEC_RANGE()

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

	VEC_RANGE()

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
bool areIntersecting(const Tetrahedron &, const FBox &);

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

template <class Scalar_, int N> struct ParametricPoint {
	using Scalar = Scalar_;
	using Vector = MakeVector<Scalar, N>;

	ParametricPoint(Vector point, Scalar param) : point(point), param(param) {}

	operator Vector() const { return point; }

	FWK_ORDER_BY(ParametricPoint, point, param);

	Vector point;
	Scalar param;
};

template <class Real, int N> struct Segment {
	static_assert(isReal<Real>(),
				  "Segment cannot be constructed using integral numbers as base type");
	using Scalar = Real;
	using Vector = MakeVector<Real, N>;
	using Point = Vector;

	Segment() : from(), to() {}
	Segment(const Point &a, const Point &b) : from(a), to(b) {}
	Segment(const pair<Point, Point> &pair) : Segment(pair.first, pair.second) {}

	template <class U>
	Segment(const Segment<U, N> &rhs) : Segment(Point(rhs.from), Point(rhs.to)) {}

	bool empty() const { return from == to; }
	Maybe<TRay<Real, N>> asRay() const {
		if(empty())
			return none;
		return TRay<Real, N>(from, normalize(to - from));
	}

	auto length() const { return fwk::distance(from, to); }
	Real lengthSq() const { return fwk::distanceSq(from, to); }

	Vector at(Real param) const { return from + (to - from) * param; }

	const Point &operator[](int n) const { return v[n]; }
	Point &operator[](int n) { return v[n]; }

	using PPoint = ParametricPoint<Real, N>;

	PPoint closestPoint(const Point &) const;
	PPoint closestPoint(const Segment &) const;
	pair<PPoint, PPoint> closestPoints(const Segment &) const;

	Real distanceSq(const Point &) const;
	Real distanceSq(const Segment &) const;

	Real distance(const Point &point) const { return std::sqrt(distanceSq(point)); }
	Real distance(const Segment &seg) const { return std::sqrt(distanceSq(seg)); }

	VEC_RANGE()
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

template <class T> EnableIfScalar<Segment2<T>, T> asXZ(const Segment3<T> &segment) {
	return {segment.from.xz(), segment.to.xz()};
}

template <class T> using Segment2Isect = Variant<None, Segment2<T>, Vector2<T>>;

Segment2Isect<float> intersection(const Segment2<float> &, const Segment2<float> &);
Segment2Isect<double> intersection(const Segment2<double> &, const Segment2<double> &);

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
	// X: edge1 Y: normal
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
		return FBox(-m_radius, 0, -m_radius, m_radius, m_height, m_radius) + m_pos;
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

	template <class T> AsRealVector<T> sampleBox(const T &min, const T &max) {
		T out;
		for(int n = 0; n < T::vector_size; n++)
			out[n] = uniform(min[n], max[n]);
		return out;
	}
	template <class T> AsRealVector<T> sampleUnitHemisphere() {
		auto point = sampleUnitSphere<T>();
		while(isZero(point))
			point = sampleUnitSphere<T>();
		return normalize(point);
	}

	template <class T> AsRealVector<T> sampleUnitSphere() {
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

	template <class T> static EnableIfScalar<Value, T> hash(T scalar) {
		return std::hash<T>()(scalar);
	}

	template <class Arg, class T>
	using EnableIfRangeNotTied =
		typename std::conditional<IsRange<T>::value && !isTied<T>(), detail::ValidType,
								  NotARange>::type::template Arg<Arg>;

	template <class T> static EnableIfRangeNotTied<Value, T> hash(const T &trange) {
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
	template <class T> static EnableIfTied<Value, T> hash(const T &object) {
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

#undef VEC_RANGE

#endif
