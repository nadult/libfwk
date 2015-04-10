/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_MATH_H
#define FWK_MATH_H

#include "fwk_base.h"
#include <cmath>

namespace fwk {

extern float g_FloatParam[16];

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

template <class TVector, class TScalar>
inline TVector lerp(const TVector &a, const TVector &b, TScalar delta) {
	DASSERT(delta >= TScalar(0) && delta <= TScalar(1));
	return (b - a) * delta + a;
}

float frand();

namespace constant {
	static const float pi = 3.14159265358979f;
	static const float e = 2.71828182845905f;
	static const float inf = 1.0f / 0.0f;
	static const float epsilon = 0.0001f;
}

struct Interval {
	Interval(float value) : min(value), max(value) {}
	Interval(float min, float max) : min(min), max(max) {}
	Interval() {}

	Interval operator+(const Interval &rhs) const { return Interval(min + rhs.min, max + rhs.max); }
	Interval operator-(const Interval &rhs) const { return Interval(min - rhs.max, max - rhs.min); }
	Interval operator*(const Interval &rhs) const;
	Interval operator*(float) const;
	Interval operator/(float val) const { return operator*(1.0f / val); }

	bool isValid() const { return min <= max; }

	float min, max;
};

Interval abs(const Interval &);
Interval floor(const Interval &);
Interval min(const Interval &, const Interval &);
Interval max(const Interval &, const Interval &);

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

	int x, y;
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


	int x, y, z;
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



	int x, y, z, w;
};

// This class should only be used for storing values, not for computation
// (unless you really know what you're doing)
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

struct float2 {
	using Scalar = float;

	float2(float x, float y) : x(x), y(y) {}
	float2(const int2 &xy) : x(xy.x), y(xy.y) {}
	float2() : x(0.0f), y(0.0f) {}
	explicit operator int2() const { return int2((int)x, (int)y); }

	float2 operator+(const float2 &rhs) const { return float2(x + rhs.x, y + rhs.y); }
	float2 operator-(const float2 &rhs) const { return float2(x - rhs.x, y - rhs.y); }
	float2 operator*(const float2 &rhs) const { return float2(x * rhs.x, y * rhs.y); }
	float2 operator*(float s) const { return float2(x * s, y * s); }
	float2 operator/(float s) const { return *this * (1.0f / s); }
	float2 operator-() const { return float2(-x, -y); }

	bool operator==(const float2 &rhs) const { return x == rhs.x && y == rhs.y; }


	float x, y;
};

float vectorToAngle(const float2 &normalized_vector);
float2 angleToVector(float radians);
float2 rotateVector(const float2 &vec, float radians);

struct float3 {
	using Scalar = float;

	float3(float x, float y, float z) : x(x), y(y), z(z) {}
	float3(const int3 &xyz) : x(xyz.x), y(xyz.y), z(xyz.z) {}
	float3() : x(0.0f), y(0.0f), z(0.0f) {}
	explicit operator int3() const { return int3((int)x, (int)y, (int)z); }

	float3 operator+(const float3 &rhs) const { return float3(x + rhs.x, y + rhs.y, z + rhs.z); }
	float3 operator-(const float3 &rhs) const { return float3(x - rhs.x, y - rhs.y, z - rhs.z); }
	float3 operator*(const float3 &rhs) const { return float3(x * rhs.x, y * rhs.y, z * rhs.z); }
	float3 operator*(float s) const { return float3(x * s, y * s, z * s); }
	float3 operator/(float s) const { return *this * (1.0f / s); }
	float3 operator-() const { return float3(-x, -y, -z); }

	bool operator==(const float3 &rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }

	float2 xy() const { return float2(x, y); }
	float2 xz() const { return float2(x, z); }
	float2 yz() const { return float2(y, z); }


	float x, y, z;
};

struct float4 {
	using Scalar = float;

	float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	float4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

	float4 operator+(const float4 &rhs) const {
		return float4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
	}
	float4 operator-(const float4 &rhs) const {
		return float4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
	}
	float4 operator*(const float4 &rhs) const {
		return float4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
	}
	float4 operator*(float s) const { return float4(x * s, y * s, z * s, w * s); }
	float4 operator/(float s) const { return *this * (1.0f / s); }
	float4 operator-() const { return float4(-x, -y, -z, -w); }

	bool operator==(const float4 &rhs) const {
		return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
	}


	float x, y, z, w;
};

struct Matrix3 {
	using Scalar = float;
	using Vector = float3;

	Matrix3(const float3 &a, const float3 &b, const float3 &c);
	Matrix3() {}

	static Matrix3 identity();

	float3 operator*(const float3 &vec) const;
	const float3 &operator[](int idx) const { return v[idx]; }
	float3 &operator[](int idx) { return v[idx]; }

	float3 v[3];
};

Matrix3 transpose(const Matrix3 &);
Matrix3 inverse(const Matrix3 &);

inline float2 inv(const float2 &vec) { return float2(1.0f / vec.x, 1.0f / vec.y); }
inline float3 inv(const float3 &vec) { return float3(1.0f / vec.x, 1.0f / vec.y, 1.0f / vec.z); }
inline float4 inv(const float4 &vec) {
	return float4(1.0f / vec.x, 1.0f / vec.y, 1.0f / vec.z, 1.0f / vec.w);
}

inline int2 min(const int2 &a, const int2 &b) { return int2(min(a.x, b.x), min(a.y, b.y)); }
inline int2 max(const int2 &a, const int2 &b) { return int2(max(a.x, b.x), max(a.y, b.y)); }
inline int3 min(const int3 &a, const int3 &b) {
	return int3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}
inline int3 max(const int3 &a, const int3 &b) {
	return int3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}

inline int2 abs(const int2 &v) { return int2(::abs(v.x), ::abs(v.y)); }
inline int3 abs(const int3 &v) { return int3(::abs(v.x), ::abs(v.y), ::abs(v.z)); }

inline float2 min(const float2 &a, const float2 &b) { return float2(min(a.x, b.x), min(a.y, b.y)); }
inline float2 max(const float2 &a, const float2 &b) { return float2(max(a.x, b.x), max(a.y, b.y)); }
inline float3 min(const float3 &a, const float3 &b) {
	return float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}
inline float3 max(const float3 &a, const float3 &b) {
	return float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}

inline int2 round(const float2 &v) { return int2(v.x + 0.5f, v.y + 0.5f); }
inline int3 round(const float3 &v) { return int3(v.x + 0.5f, v.y + 0.5f, v.z + 0.5f); }

class Ray {
  public:
	Ray(const float3 &origin, const float3 &dir) : m_origin(origin), m_dir(dir) {
		m_inv_dir = float3(1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z);
	}
	Ray(const float3 &origin, const float3 &dir, const float3 &idir)
		: m_origin(origin), m_dir(dir), m_inv_dir(idir) {}
	Ray() {}

	const float3 &dir() const { return m_dir; }
	const float3 &invDir() const { return m_inv_dir; }
	const float3 &origin() const { return m_origin; }
	const float3 at(float t) const { return m_origin + m_dir * t; }
	const Ray operator-() const { return Ray(m_origin, -m_dir, -m_inv_dir); }

  private:
	float3 m_origin;
	float3 m_dir;
	float3 m_inv_dir;
};

struct Segment : public Ray {
	Segment(const Ray &ray, float min = -constant::inf, float max = constant::inf);
	Segment(const float3 &source, const float3 &target);
	Segment() : m_min(0.0f), m_max(constant::inf) {}

	float min() const { return m_min; }
	float max() const { return m_max; }

  protected:
	float m_min, m_max;
};

class Plane {
  public:
	Plane() {}
	Plane(const float3 &normalized_dir, float distance);
	Plane(const float3 &origin, const float3 &normalized_dir);
	Plane(const float3 &a, const float3 &b, const float3 &c);

	const float3 origin() const { return m_dir * m_distance; }
	const float3 &normal() const { return m_dir; }
	const float3 &dir() const { return m_dir; }
	float distance() const { return m_distance; }

  protected:
	float3 m_dir;
	float m_distance;
};

float3 project(const float3 &point, const Plane &plane);

template <class TVec2> struct Rect {
	using Vector = TVec2;
	using Scalar = typename TVec2::Scalar;

	template <class T> explicit Rect(const Rect<T> &other) : min(other.min), max(other.max) {}
	explicit Rect(const Vector &size) : min(0, 0), max(size) {}
	Rect(Vector min, Vector max) : min(min), max(max) {}
	Rect(Scalar minX, Scalar minY, Scalar maxX, Scalar maxY) : min(minX, minY), max(maxX, maxY) {}
	Rect() = default;
	static Rect empty() { return Rect(Vector(), Vector()); }

	Rect subRect(const Rect &part) const {
		return Rect(lerp(min.x, max.x, part.min.x), lerp(min.y, max.y, part.min.y),
					lerp(min.x, max.x, part.max.x), lerp(min.x, max.x, part.max.y));
	}

	Scalar width() const { return max.x - min.x; }
	Scalar height() const { return max.y - min.y; }
	Vector size() const { return max - min; }
	Vector center() const { return (max + min) / Scalar(2); }
	Scalar surfaceArea() const { return (max.x - min.x) * (max.y - min.y); }

	void setWidth(Scalar width) { max.x = min.x + width; }
	void setHeight(Scalar height) { max.y = min.y + height; }
	void setSize(const Vector &size) { max = min + size; }

	Rect operator+(const Vector &offset) const { return Rect(min + offset, max + offset); }
	Rect operator-(const Vector &offset) const { return Rect(min - offset, max - offset); }
	Rect operator*(Scalar scale) const { return Rect(min * scale, max * scale); }

	Rect operator+(const Rect &rhs) { return Rect(fwk::min(min, rhs.min), fwk::max(max, rhs.max)); }

	bool isEmpty() const { return max.x <= min.x || max.y <= min.y; }
	bool isInside(const Vector &point) const {
		return point.x >= min.x && point.x < max.x && point.y >= min.y && point.y < max.y;
	}

	bool operator==(const Rect &rhs) const { return min == rhs.min && max == rhs.max; }

	Vector min, max;
};

template <class TVec2>
inline Rect<TVec2> inset(Rect<TVec2> rect, const TVec2 &tl, const TVec2 &br) {
	return Rect<TVec2>(rect.min + tl, rect.max - br);
}

template <class TVec2> inline Rect<TVec2> inset(Rect<TVec2> rect, const TVec2 &inset) {
	return Rect<TVec2>(rect.min + inset, rect.max - inset);
}

template <class TVec2> bool operator==(const Rect<TVec2> &lhs, const Rect<TVec2> &rhs) {
	return lhs.min == rhs.min && lhs.max == rhs.max;
}

template <class TVec2> const Rect<TVec2> sum(const Rect<TVec2> &a, const Rect<TVec2> &b) {
	return Rect<TVec2>(min(a.min, b.min), max(a.max, b.max));
}

template <class TVec2> const Rect<TVec2> intersection(const Rect<TVec2> &a, const Rect<TVec2> &b) {
	return Rect<TVec2>(max(a.min, b.min), min(a.max, b.max));
}

template <class TVec3> struct Box {
	using Vector = TVec3;
	using Scalar = typename TVec3::Scalar;

	template <class TType3>
	explicit Box(const Box<TType3> &other)
		: min(other.min), max(other.max) {}
	Box(Vector min, Vector max) : min(min), max(max) {}
	Box(Scalar minX, Scalar minY, Scalar minZ, Scalar maxX, Scalar maxY, Scalar maxZ)
		: min(minX, minY, minZ), max(maxX, maxY, maxZ) {}
	Box() = default;
	static Box empty() { return Box(Vector(), Vector()); }

	Scalar width() const { return max.x - min.x; }
	Scalar height() const { return max.y - min.y; }
	Scalar depth() const { return max.z - min.z; }
	Vector size() const { return max - min; }
	Vector center() const { return (max + min) / Scalar(2); }

	Box operator+(const Vector &offset) const { return Box(min + offset, max + offset); }
	Box operator-(const Vector &offset) const { return Box(min - offset, max - offset); }

	bool isEmpty() const { return max.x <= min.x || max.y <= min.y || max.z <= min.z; }
	bool isInside(const Vector &point) const {
		return point.x >= min.x && point.x < max.x && point.y >= min.y && point.y < max.y &&
			   point.z >= min.z && point.z < max.z;
	}

	void getCorners(Vector corners[8]) const {
		for(int n = 0; n < 8; n++) {
			corners[n].x = (n & 4 ? min : max).x;
			corners[n].y = (n & 2 ? min : max).y;
			corners[n].z = (n & 1 ? min : max).z;
		}
	}

	Vector closestPoint(const Vector &point) const {
		return Vector(point.x < min.x ? min.x : point.x > max.x ? max.x : point.x,
					  point.y < min.y ? min.y : point.y > max.y ? max.y : point.y,
					  point.z < min.z ? min.z : point.z > max.z ? max.z : point.z);
	}

	bool operator==(const Box &rhs) const { return min == rhs.min && max == rhs.max; }

	Vector min, max;
};

template <class TVec3> const Box<TVec3> sum(const Box<TVec3> &a, const Box<TVec3> &b) {
	return Box<TVec3>(min(a.min, b.min), max(a.max, b.max));
}

template <class TVec3> const Box<TVec3> intersection(const Box<TVec3> &a, const Box<TVec3> &b) {
	return Box<TVec3>(max(a.min, b.min), min(a.max, b.max));
}

Box<int3> enclosingIBox(const Box<float3> &);
Box<float3> rotateY(const Box<float3> &box, const float3 &origin, float angle);

// returns infinity if doesn't intersect
float intersection(const Ray &ray, const Box<float3> &box);
float intersection(const Interval idir[3], const Interval origin[3], const Box<float3> &box);
float intersection(const Segment &segment, const Box<float3> &box);

template <class T> bool areOverlapping(const Rect<T> &a, const Rect<T> &b) {
	return (b.min.x < a.max.x && a.min.x < b.max.x) && (b.min.y < a.max.y && a.min.y < b.max.y);
}

inline bool areOverlapping(const Rect<int2> &a, const Rect<int2> &b) {
	return (((b.min.x - a.max.x) & (a.min.x - b.max.x)) &
			((b.min.y - a.max.y) & (a.min.y - b.max.y))) >>
		   31;
}

template <class T> bool areOverlapping(const Rect<T> &a, const Rect<T> &b);

template <class T> bool areOverlapping(const Box<T> &a, const Box<T> &b);

bool areAdjacent(const Rect<int2> &, const Rect<int2> &);
float distanceSq(const Rect<float2> &, const Rect<float2> &);
float distanceSq(const Box<float3> &, const Box<float3> &);
inline float distance(const Box<float3> &a, const Box<float3> &b) {
	return sqrtf(distanceSq(a, b));
}

bool isInsideFrustum(const float3 &eye_pos, const float3 &eye_dir, float min_dot,
					 const Box<float3> &box);

typedef Rect<int2> IRect;
typedef Rect<float2> FRect;
typedef Box<int3> IBox;
typedef Box<float3> FBox;

inline int3 asXZ(const int2 &pos) { return int3(pos.x, 0, pos.y); }
inline int3 asXY(const int2 &pos) { return int3(pos.x, pos.y, 0); }
inline int3 asXZY(const int2 &pos, int y) { return int3(pos.x, y, pos.y); }
inline int3 asXYZ(const int2 &pos, int z) { return int3(pos.x, pos.y, z); }

inline float3 asXZ(const float2 &pos) { return float3(pos.x, 0, pos.y); }
inline float3 asXY(const float2 &pos) { return float3(pos.x, pos.y, 0); }
inline float3 asXZY(const float2 &pos, float y) { return float3(pos.x, y, pos.y); }
inline float3 asXYZ(const float2 &pos, float z) { return float3(pos.x, pos.y, z); }

float dot(const float2 &a, const float2 &b);
float dot(const float3 &a, const float3 &b);
float dot(const float4 &a, const float4 &b);
float3 cross(const float3 &a, const float3 &b);

float lengthSq(const float2 &);
float lengthSq(const float3 &);
float distanceSq(const float3 &, const float3 &);
float distanceSq(const float2 &, const float2 &);

float length(const float2 &);
float length(const float3 &);
float distance(const float3 &, const float3 &);
float distance(const float2 &, const float2 &);

float angleDistance(float ang1, float ang2);
float blendAngles(float initial, float target, float step);

// TODO: this doesn't work properly if max is smaller than min
template <class T> inline T clamp(T obj, T min, T max) { return fwk::min(fwk::max(obj, min), max); }

template <class TVec> const TVec normalized(const TVec &vec) { return vec / length(vec); }
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

#endif
