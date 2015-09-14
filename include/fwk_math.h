/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#ifndef FWK_MATH_H
#define FWK_MATH_H

#include "fwk_base.h"
#include <cmath>

namespace fwk {

// TODO: make sure that all classes / structures here have proper default constructor (for example
// AxisAngle requires fixing)

namespace constant {
	static const float pi = 3.14159265358979f;
	static const float e = 2.71828182845905f;
	static const float inf = 1.0f / 0.0f;
	static const float epsilon = 0.0001f;
}

template <class T> inline const T clamp(T obj, T tmin, T tmax) { return min(tmax, max(tmin, obj)); }

template <class T, class T1> inline T &operator+=(T &a, const T1 &b) {
	a = a + b;
	return a;
}
template <class T, class T1> inline T &operator-=(T &a, const T1 &b) {
	a = a - b;
	return a;
}
template <class T, class T1> inline T &operator*=(T &a, const T1 &b) {
	a = a * b;
	return a;
}
template <class T, class T1> inline T &operator/=(T &a, const T1 &b) {
	a = a / b;
	return a;
}

inline float degToRad(float v) { return v * (2.0f * constant::pi / 360.0f); }
inline float radToDeg(float v) { return v * (360.0f / (2.0f * constant::pi)); }

// Return angle in range (0; 2 * PI)
float normalizeAngle(float radians);

template <class Obj, class Scalar> inline Obj lerp(const Obj &a, const Obj &b, const Scalar &x) {
	return (b - a) * x + a;
}

struct int2 {
	using Scalar = int;

	int2(int x, int y) : x(x), y(y) {}
	int2() : x(0), y(0) {}

	int2 operator+(const int2 &rhs) const { return int2(x + rhs.x, y + rhs.y); }
	int2 operator-(const int2 &rhs) const { return int2(x - rhs.x, y - rhs.y); }
	int2 operator*(int s) const { return int2(x * s, y * s); }
	int2 operator/(int s) const { return int2(x / s, y / s); }
	int2 operator%(int s) const { return int2(x % s, y % s); }
	int2 operator-() const { return int2(-x, -y); }

	bool operator==(const int2 &rhs) const { return x == rhs.x && y == rhs.y; }

	int &operator[](int idx) { return v[idx]; }
	const int &operator[](int idx) const { return v[idx]; }

	union {
		struct {
			int x, y;
		};
		int v[2];
	};
};

struct short2 {
	using Scalar = short;

	short2(short x, short y) : x(x), y(y) {}
	short2(const int2 &rhs) : x(rhs.x), y(rhs.y) {}
	short2() : x(0), y(0) {}
	operator int2() const { return int2(x, y); }

	short2 operator+(const short2 &rhs) const { return short2(x + rhs.x, y + rhs.y); }
	short2 operator-(const short2 &rhs) const { return short2(x - rhs.x, y - rhs.y); }
	short2 operator*(short s) const { return short2(x * s, y * s); }
	short2 operator/(short s) const { return short2(x / s, y / s); }
	short2 operator%(short s) const { return short2(x % s, y % s); }
	short2 operator-() const { return short2(-x, -y); }

	bool operator==(const short2 &rhs) const { return x == rhs.x && y == rhs.y; }

	short x, y;
};

struct int3 {
	using Scalar = int;

	int3(int x, int y, int z) : x(x), y(y), z(z) {}
	int3() : x(0), y(0), z(0) {}

	int3 operator+(const int3 &rhs) const { return int3(x + rhs.x, y + rhs.y, z + rhs.z); }
	int3 operator-(const int3 &rhs) const { return int3(x - rhs.x, y - rhs.y, z - rhs.z); }
	int3 operator*(const int3 &rhs) const { return int3(x * rhs.x, y * rhs.y, z * rhs.z); }
	int3 operator*(int s) const { return int3(x * s, y * s, z * s); }
	int3 operator/(int s) const { return int3(x / s, y / s, z / s); }
	int3 operator%(int s) const { return int3(x % s, y % s, z % s); }

	bool operator==(const int3 &rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }

	int2 xy() const { return int2(x, y); }
	int2 xz() const { return int2(x, z); }
	int2 yz() const { return int2(y, z); }

	int &operator[](int idx) { return v[idx]; }
	const int &operator[](int idx) const { return v[idx]; }

	union {
		struct {
			int x, y, z;
		};
		int v[3];
	};
};

struct int4 {
	using Scalar = int;

	int4(int x, int y, int z, int w) : x(x), y(y), z(z), w(w) {}
	int4() : x(0), y(0), z(0), w(0) {}

	int4 operator+(const int4 rhs) const {
		return int4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
	}
	int4 operator-(const int4 rhs) const {
		return int4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
	}
	int4 operator*(int s) const { return int4(x * s, y * s, z * s, w * s); }
	int4 operator/(int s) const { return int4(x / s, y / s, z / s, w / s); }

	bool operator==(const int4 &rhs) const {
		return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
	}

	int &operator[](int idx) { return v[idx]; }
	const int &operator[](int idx) const { return v[idx]; }

	union {
		struct {
			int x, y, z, w;
		};
		int v[4];
	};
};

struct float2 {
	using Scalar = float;

	float2(float x, float y) : x(x), y(y) {}
	float2(const int2 &xy) : x(xy.x), y(xy.y) {}
	float2() : x(0.0f), y(0.0f) {}
	explicit operator int2() const { return int2((int)x, (int)y); }

	float2 operator+(const float2 &rhs) const { return float2(x + rhs.x, y + rhs.y); }
	float2 operator*(const float2 &rhs) const { return float2(x * rhs.x, y * rhs.y); }
	float2 operator/(const float2 &rhs) const { return float2(x / rhs.x, y / rhs.y); }
	float2 operator-(const float2 &rhs) const { return float2(x - rhs.x, y - rhs.y); }
	float2 operator*(float s) const { return float2(x * s, y * s); }
	float2 operator/(float s) const { return *this * (1.0f / s); }
	float2 operator-() const { return float2(-x, -y); }

	bool operator==(const float2 &rhs) const { return x == rhs.x && y == rhs.y; }

	float &operator[](int idx) { return v[idx]; }
	const float &operator[](int idx) const { return v[idx]; }

	union {
		struct {
			float x, y;
		};
		float v[2];
	};
};

struct float3 {
	using Scalar = float;

	float3(float x, float y, float z) : x(x), y(y), z(z) {}
	float3(const float2 &xy, float z) : x(xy.x), y(xy.y), z(z) {}
	float3(const int3 &xyz) : x(xyz.x), y(xyz.y), z(xyz.z) {}
	float3() : x(0.0f), y(0.0f), z(0.0f) {}
	explicit operator int3() const { return int3((int)x, (int)y, (int)z); }

	float3 operator*(const float3 &rhs) const { return float3(x * rhs.x, y * rhs.y, z * rhs.z); }
	float3 operator/(const float3 &rhs) const { return float3(x / rhs.x, y / rhs.y, z / rhs.z); }
	float3 operator+(const float3 &rhs) const { return float3(x + rhs.x, y + rhs.y, z + rhs.z); }
	float3 operator-(const float3 &rhs) const { return float3(x - rhs.x, y - rhs.y, z - rhs.z); }
	float3 operator*(float s) const { return float3(x * s, y * s, z * s); }
	float3 operator/(float s) const { return *this * (1.0f / s); }
	float3 operator-() const { return float3(-x, -y, -z); }

	void operator*=(float s) {
		x *= s;
		y *= s;
		z *= s;
	}

	bool operator==(const float3 &rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }

	float &operator[](int idx) { return v[idx]; }
	const float &operator[](int idx) const { return v[idx]; }

	float2 xy() const { return float2(x, y); }
	float2 xz() const { return float2(x, z); }
	float2 yz() const { return float2(y, z); }

	union {
		struct {
			float x, y, z;
		};
		float v[3];
	};
};

struct float4 {
	using Scalar = float;

	// TODO: use range[4]
	float4(const float *v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {}
	float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	explicit float4(const float3 &xyz, float w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
	explicit float4(const float2 &xy, float z, float w) : x(xy.x), y(xy.y), z(z), w(w) {}
	float4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

	float4 operator*(const float4 &rhs) const {
		return float4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
	}
	float4 operator/(const float4 &rhs) const {
		return float4(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w);
	}
	float4 operator+(const float4 &rhs) const {
		return float4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
	}
	float4 operator-(const float4 &rhs) const {
		return float4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
	}
	float4 operator*(float s) const { return float4(x * s, y * s, z * s, w * s); }
	float4 operator/(float s) const { return *this * (1.0f / s); }
	float4 operator-() const { return float4(-x, -y, -z, -w); }

	bool operator==(const float4 &rhs) const {
		return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
	}

	float &operator[](int idx) { return v[idx]; }
	const float &operator[](int idx) const { return v[idx]; }

	float2 xy() const { return float2(x, y); }
	float2 xz() const { return float2(x, z); }
	float2 yz() const { return float2(y, z); }
	float3 xyz() const { return float3(x, y, z); }

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

const int2 min(const int2 &a, const int2 &b);
const int2 max(const int2 &a, const int2 &b);
const int3 min(const int3 &a, const int3 &b);
const int3 max(const int3 &a, const int3 &b);

const int2 abs(const int2 &v);
const int3 abs(const int3 &v);

inline const float2 min(const float2 &a, const float2 &b) {
	return float2(min(a.x, b.x), min(a.y, b.y));
}
inline const float2 max(const float2 &a, const float2 &b) {
	return float2(max(a.x, b.x), max(a.y, b.y));
}
inline const float3 min(const float3 &a, const float3 &b) {
	return float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}
inline const float3 max(const float3 &a, const float3 &b) {
	return float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}

inline const int2 round(const float2 &v) { return int2(v.x + 0.5f, v.y + 0.5f); }
inline const int3 round(const float3 &v) { return int3(v.x + 0.5f, v.y + 0.5f, v.z + 0.5f); }

inline const int2 ceil(const float2 &v) {
	return int2(v.x + (1.0f - constant::epsilon), v.y + (1.0f - constant::epsilon));
}
inline const int3 ceil(const float3 &v) {
	return int3(v.x + (1.0f - constant::epsilon), v.y + (1.0f - constant::epsilon),
				v.z + (1.0f - constant::epsilon));
}

inline const int3 asXZ(const int2 &pos) { return int3(pos.x, 0, pos.y); }
inline const int3 asXY(const int2 &pos) { return int3(pos.x, pos.y, 0); }
inline const int3 asXZY(const int2 &pos, int y) { return int3(pos.x, y, pos.y); }

inline const float3 asXZ(const float2 &pos) { return float3(pos.x, 0, pos.y); }
inline const float3 asXY(const float2 &pos) { return float3(pos.x, pos.y, 0); }
inline const float3 asXZY(const float2 &pos, float y) { return float3(pos.x, y, pos.y); }
inline const float3 asXZY(const float3 &pos) { return float3(pos.x, pos.z, pos.y); }

float dot(const float2 &a, const float2 &b);
float dot(const float3 &a, const float3 &b);
float dot(const float4 &a, const float4 &b);

float lengthSq(const float2 &);
float lengthSq(const float3 &);
float lengthSq(const float4 &);
float distanceSq(const float2 &, const float2 &);
float distanceSq(const float3 &, const float3 &);
float distanceSq(const float4 &, const float4 &);

float length(const float2 &);
float length(const float3 &);
float length(const float4 &);
float distance(const float2 &, const float2 &);
float distance(const float3 &, const float3 &);
float distance(const float4 &, const float4 &);

float2 inverse(const float2 &);
float3 inverse(const float3 &);
float4 inverse(const float4 &);

template <class T> bool areSimilar(const T &a, const T &b) {
	return distanceSq(a, b) < constant::epsilon;
}

const float2 normalize(const float2 &);
const float3 normalize(const float3 &);
const float3 cross(const float3 &a, const float3 &b);

const float2 inv(const float2 &);
const float3 inv(const float3 &);
const float4 inv(const float4 &);

float vectorToAngle(const float2 &normalized_vector);
const float2 angleToVector(float radians);
const float2 rotateVector(const float2 &vec, float radians);

const float3 rotateVector(const float3 &pos, const float3 &axis, float angle);
template <class TVec> bool isNormalized(const TVec &vec) {
	auto length_sq = lengthSq(vec);
	return length_sq >= 1.0f - constant::epsilon && length_sq <= 1.0f + constant::epsilon;
}

float triangleArea(const float3 &, const float3 &, const float3 &);
float frand();
float angleDistance(float a, float b);
float blendAngles(float initial, float target, float step);
float fixAngle(float angle);

bool isnan(float);
bool isnan(const float2 &);
bool isnan(const float3 &);
bool isnan(const float4 &);

class Matrix3;
class Matrix4;

template <class TVec2> struct Rect {
	using Vec2 = TVec2;
	using Scalar = typename TVec2::Scalar;

	template <class TTVec2>
	explicit Rect(const Rect<TTVec2> &other)
		: min(other.min), max(other.max) {}
	explicit Rect(Vec2 size) : min(0, 0), max(size) {}
	Rect(Vec2 min, Vec2 max) : min(min), max(max) {}
	Rect(Scalar minX, Scalar minY, Scalar maxX, Scalar maxY) : min(minX, minY), max(maxX, maxY) {}
	Rect() = default;
	static Rect empty() { return Rect(Vec2(), Vec2()); }

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
	Rect operator*(Scalar scale) const { return Rect(min * scale, max * scale); }

	Rect operator+(const Rect &rhs) { return Rect(fwk::min(min, rhs.min), fwk::max(max, rhs.max)); }

	void include(const Vec2 &point) {
		min = fwk::min(min, point);
		max = fwk::max(max, point);
	}

	// Returns corners in clockwise order
	// TODO: CW or CCW depends on handeness...
	void getCorners(TRange<Vec2, 4>) const;

	bool isEmpty() const { return max.x <= min.x || max.y <= min.y; }

	// TODO: remove or specialize for int/float
	bool isInside(const Vec2 &point) const {
		return point.x >= min.x && point.x < max.x && point.y >= min.y && point.y < max.y;
	}

	bool operator==(const Rect &rhs) const { return min == rhs.min && max == rhs.max; }

	Vec2 min, max;
};

template <class TVec2> const Rect<TVec2> sum(const Rect<TVec2> &a, const Rect<TVec2> &b) {
	return Rect<TVec2>(min(a.min, b.min), max(a.max, b.max));
}

template <class TVec2> const Rect<TVec2> intersection(const Rect<TVec2> &a, const Rect<TVec2> &b) {
	return Rect<TVec2>(max(a.min, b.min), min(a.max, b.max));
}

bool areOverlapping(const Rect<float2> &a, const Rect<float2> &b);
bool areOverlapping(const Rect<int2> &a, const Rect<int2> &b);

bool areAdjacent(const Rect<int2> &, const Rect<int2> &);
float distanceSq(const Rect<float2> &, const Rect<float2> &);

template <class TVec3> struct Box {
	using Vec3 = TVec3;
	using Vec2 = decltype(TVec3().xz());
	using Scalar = typename TVec3::Scalar;

	template <class TTVec3>
	explicit Box(const Box<TTVec3> &other)
		: min(other.min), max(other.max) {}
	explicit Box(Vec3 size) : min(0, 0, 0), max(size) {}
	Box(Vec3 min, Vec3 max) : min(min), max(max) {}
	Box(Scalar minX, Scalar minY, Scalar minZ, Scalar maxX, Scalar maxY, Scalar maxZ)
		: min(minX, minY, minZ), max(maxX, maxY, maxZ) {}
	Box(CRange<Vec3>);
	Box() = default;
	static Box empty() { return Box(Vec3(), Vec3()); }

	Scalar width() const { return max.x - min.x; }
	Scalar height() const { return max.y - min.y; }
	Scalar depth() const { return max.z - min.z; }
	Vec3 size() const { return max - min; }
	Vec3 center() const { return (max + min) / Scalar(2); }

	Box operator+(const Vec3 &offset) const { return Box(min + offset, max + offset); }
	Box operator-(const Vec3 &offset) const { return Box(min - offset, max - offset); }
	Box operator*(const Vec3 &scale) const { return Box(min * scale, max * scale); }
	Box operator*(Scalar scale) const { return Box(min * scale, max * scale); }

	void include(const Vec3 &point) {
		min = Vec3(fwk::min(min.x, point.x), fwk::min(min.y, point.y), fwk::min(min.z, point.z));
		max = Vec3(fwk::max(max.x, point.x), fwk::max(max.y, point.y), fwk::max(max.z, point.z));
	}
	void enlarge(float v) {
		min -= Vec3(v, v, v);
		max += Vec3(v, v, v);
	}

	// TODO: what about bounding boxes enclosing single point?
	bool isEmpty() const { return max.x <= min.x || max.y <= min.y || max.z <= min.z; }

	// TODO: remove or specialize for int/float
	bool isInside(const Vec3 &point) const {
		return point.x >= min.x && point.x < max.x && point.y >= min.y && point.y < max.y &&
			   point.z >= min.z && point.z < max.z;
	}

	void getCorners(TRange<Vec3, 8>) const;

	const Rect<Vec2> xz() const { return Rect<Vec2>(min.xz(), max.xz()); }
	const Rect<Vec2> xy() const { return Rect<Vec2>(min.xy(), max.xy()); }
	const Rect<Vec2> yz() const { return Rect<Vec2>(min.yz(), max.yz()); }

	bool operator==(const Box &rhs) const { return min == rhs.min && max == rhs.max; }

	Vec3 min, max;
};

template <class TVec3> const Box<TVec3> sum(const Box<TVec3> &a, const Box<TVec3> &b) {
	return Box<TVec3>(min(a.min, b.min), max(a.max, b.max));
}

template <class TVec3> const Box<TVec3> intersection(const Box<TVec3> &a, const Box<TVec3> &b) {
	return Box<TVec3>(max(a.min, b.min), min(a.max, b.max));
}

const Box<int3> enclosingIBox(const Box<float3> &);
const Box<float3> rotateY(const Box<float3> &box, const float3 &origin, float angle);

bool areOverlapping(const Box<float3> &a, const Box<float3> &b);
bool areOverlapping(const Box<int3> &a, const Box<int3> &b);

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

  protected:
	float3 v[3];
};

bool operator==(const Matrix3 &lhs, const Matrix3 &rhs);

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
	Matrix4(const float *vals) { // TODO: use range<float, 16>
		for(int n = 0; n < 4; n++)
			v[n] = float4(vals + n * 4);
	}

	static const Matrix4 identity();

	const float4 row(int n) const { return float4(v[0][n], v[1][n], v[2][n], v[3][n]); }

	float operator()(int row, int col) const { return v[col][row]; }
	float &operator()(int row, int col) { return v[col][row]; }

	const float4 &operator[](int n) const { return v[n]; }
	float4 &operator[](int n) { return v[n]; }

	const Matrix4 operator+(const Matrix4 &) const;
	const Matrix4 operator-(const Matrix4 &) const;
	const Matrix4 operator*(float) const;

  protected:
	float4 v[4];
};

bool operator==(const Matrix4 &lhs, const Matrix4 &rhs);

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

  protected:
	float3 m_axis;
	float m_angle;
};

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

	const Quat operator*(const Quat &) const;
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

	float3 translation;
	float3 scale;
	Quat rotation;
};

AffineTrans operator*(const AffineTrans &, const AffineTrans &);
AffineTrans lerp(const AffineTrans &, const AffineTrans &, float t);

class Triangle {
  public:
	Triangle(const float3 &a, const float3 &b, const float3 &c);
	Triangle() : Triangle(float3(), float3(), float3()) {}

	float3 operator[](int idx) const {
		DASSERT(idx >= 0 && idx < 3);
		return idx == 0 ? a() : idx == 1 ? b() : c();
	}

	float3 a() const { return m_point; }
	float3 b() const { return m_point + m_edge[0]; }
	float3 c() const { return m_point + m_edge[1]; }
	float3 center() const { return m_point + (m_edge[0] + m_edge[1]) * (1.0f / 3.0f); }
	const float3 &cross() const { return m_cross; }

	Triangle operator*(float scale) const {
		return Triangle(a() * scale, b() * scale, c() * scale);
	}
	Triangle operator*(const float3 &scale) const {
		return Triangle(a() * scale, b() * scale, c() * scale);
	}

	float3 edge1() const { return m_edge[0]; }
	float3 edge2() const { return m_edge[1]; }
	float3 normal() const;
	float area() const;

  protected:
	float3 m_point;
	float3 m_edge[2];
	float3 m_cross;
};

struct Triangle2D {
	Triangle2D(const float2 &a, const float2 &b, const float2 &c) : m_points({{a, b, c}}) {}

	const float2 &operator[](int idx) const { return m_points[idx]; }
	float2 center() const { return (m_points[0] + m_points[1] + m_points[2]) * 0.5f; }

  private:
	array<float2, 3> m_points;
};

Triangle operator*(const Matrix4 &, const Triangle &);

float distance(const Triangle &, const Triangle &);
float distance(const Triangle &, const float3 &);

bool areIntersecting(const Triangle &, const Triangle &);
bool areIntersecting(const Triangle2D &, const Triangle2D &);

// dot(plane.normal(), pointOnPlane) == plane.distance();
class Plane {
  public:
	Plane() {}
	Plane(const float3 &normal, float distance) : m_nrm(normal), m_dist(distance) {}

	// TODO: should triangle be CW or CCW?
	explicit Plane(const Triangle &);
	Plane(const float3 &a, const float3 &b, const float3 &c) : Plane(Triangle(a, b, c)) {}

	const float3 &normal() const { return m_nrm; }

	// distance from if dir is normalized then it is a distance
	// from plane to point (0, 0, 0)
	float distance() const { return m_dist; }

  protected:
	float3 m_nrm;
	float m_dist;
};

class Tetrahedron {
  public:
	Tetrahedron(const float3 &p1, const float3 &p2, const float3 &p3, const float3 &p4);
	Tetrahedron(TRange<const float3, 4> points)
		: Tetrahedron(points[0], points[1], points[2], points[3]) {}
	Tetrahedron() : Tetrahedron(float3(), float3(), float3(), float3()) {}

	float volume() const;
	bool isIntersecting(const Triangle &) const;
	bool isValid() const;

	const float3 &operator[](int idx) const { return m_corners[idx]; }
	const float3 &corner(int idx) const { return m_corners[idx]; }
	const auto &corners() const { return m_corners; }

  private:
	array<float3, 4> m_corners;
};

const Plane normalize(const Plane &);
const Plane operator*(const Matrix4 &, const Plane &);
float dot(const Plane &, const float3 &point);

class Ray {
  public:
	Ray(const float3 &origin = float3(0, 0, 0), const float3 &dir = float3(0, 0, 1));
	Ray(const Matrix4 &screen_to_world, const float2 &screen_pos);
	Ray(const float3 &origin, const float3 &dir, const float3 &idir)
		: m_origin(origin), m_dir(dir), m_inv_dir(idir) {}

	const float3 &dir() const { return m_dir; }
	const float3 &invDir() const { return m_inv_dir; }
	const float3 &origin() const { return m_origin; }
	const float3 at(float t) const { return m_origin + m_dir * t; }
	const Ray operator-() const { return Ray(m_origin, -m_dir, -m_inv_dir); }

  protected:
	float3 m_origin;
	float3 m_dir;
	float3 m_inv_dir;
};

class Segment : public Ray {
  public:
	Segment(const float3 &start = float3(), const float3 &end = float3(0, 0, 1));

	float length() const { return m_length; }
	float3 end() const { return m_end; }

  private:
	float3 m_end;
	float m_length;
};

float distance(const Triangle &tri, const Segment &);

// returns infinity if doesn't intersect
pair<float, float> intersectionRange(const Ray &, const Box<float3> &box);
pair<float, float> intersectionRange(const Segment &, const Box<float3> &box);

inline float intersection(const Ray &ray, const Box<float3> &box) {
	return intersectionRange(ray, box).first;
}

inline float intersection(const Segment &segment, const Box<float3> &box) {
	return intersectionRange(segment, box).first;
}

float intersection(const Ray &, const Triangle &);
float intersection(const Segment &, const Triangle &);

const Segment operator*(const Matrix4 &, const Segment &);

float intersection(const Segment &, const Plane &);
float intersection(const Ray &, const Plane &);

inline float intersection(const Plane &plane, const Segment &segment) {
	return intersection(segment, plane);
}
inline float intersection(const Plane &plane, const Ray &ray) { return intersection(ray, plane); }
inline float intersection(const Triangle &tri, const Segment &segment) {
	return intersection(segment, tri);
}

bool intersection(const Plane &, const Plane &, Ray &out);

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

	Frustum() {}
	Frustum(const Matrix4 &view_projection);
	Frustum(TRange<Plane, planes_count>);

	bool isIntersecting(const float3 &point) const;
	bool isIntersecting(const FBox &box) const;

	// TODO: clang crashes on this (in full opt with -DNDEBUG)
	bool isIntersecting(CRange<float3> points) const;

	const Plane &operator[](int idx) const { return m_planes[idx]; }
	Plane &operator[](int idx) { return m_planes[idx]; }
	int size() const { return planes_count; }

  protected:
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

  private:
	float3 m_pos;
	float m_radius;
	float m_height;
};

float distance(const Cylinder &, const float3 &);
bool areIntersecting(const Cylinder &, const Cylinder &);
bool areIntersecting(const FBox &, const Cylinder &);

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
SERIALIZE_AS_POD(Segment)

#endif
