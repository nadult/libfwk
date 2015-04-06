/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_math.h"
#include <cstdlib>

namespace fwk {

float g_FloatParam[16];

#ifdef _WIN32
void sincosf(float rad, float *s, float *c) {
	DASSERT(s && c);
	*s = sin(rad);
	*c = cos(rad);
}
#endif

float frand() { return float(rand()) / float(RAND_MAX); }

Interval Interval::operator*(const Interval &rhs) const {
	float a = min * rhs.min, b = min * rhs.max;
	float c = max * rhs.min, d = max * rhs.max;

	return Interval(fwk::min(fwk::min(a, b), fwk::min(c, d)),
					fwk::max(fwk::max(a, b), fwk::max(c, d)));
}

Interval Interval::operator*(float val) const {
	float tmin = min * val, tmax = max * val;
	return val < 0 ? Interval(tmax, tmin) : Interval(tmin, tmax);
}

Interval abs(const Interval &value) {
	if(value.min < 0.0f)
		return value.max < 0.0f ? Interval(-value.max, -value.min)
								: Interval(0.0f, max(-value.min, value.max));
	return value;
}

Interval floor(const Interval &value) { return Interval(floorf(value.min), floorf(value.max)); }

Interval min(const Interval &lhs, const Interval &rhs) {
	return Interval(min(lhs.min, rhs.min), min(lhs.max, rhs.max));
}

Interval max(const Interval &lhs, const Interval &rhs) {
	return Interval(max(lhs.min, rhs.min), max(lhs.max, rhs.max));
}

template <class T> bool areOverlapping(const Box<T> &a, const Box<T> &b) {
	return (b.min.x < a.max.x && a.min.x < b.max.x) && (b.min.y < a.max.y && a.min.y < b.max.y) &&
		   (b.min.z < a.max.z && a.min.z < b.max.z);
}

template bool areOverlapping<int3>(const Box<int3> &, const Box<int3> &);
template bool areOverlapping<float3>(const Box<float3> &, const Box<float3> &);

float vectorToAngle(const float2 &normalized_vec) {
	float ang = acos(normalized_vec.x);
	return normalized_vec.y < 0 ? 2.0f * constant::pi - ang : ang;
}

float2 angleToVector(float radians) {
	float s, c;
	sincosf(radians, &s, &c);
	return float2(c, s);
}

float2 rotateVector(const float2 &vec, float radians) {
	float s, c;
	sincosf(radians, &s, &c);
	return float2(c * vec.x - s * vec.y, c * vec.y + s * vec.x);
}

Box<int3> enclosingIBox(const Box<float3> &fbox) {
	return Box<int3>(floorf(fbox.min.x), floorf(fbox.min.y), floorf(fbox.min.z), ceilf(fbox.max.x),
					 ceilf(fbox.max.y), ceilf(fbox.max.z));
};

Box<float3> rotateY(const Box<float3> &box, const float3 &origin, float angle) {
	float3 corners[8];
	box.getCorners(corners);
	float2 xz_origin = origin.xz();

	for(int n = 0; n < arraySize(corners); n++)
		corners[n] =
			asXZY(rotateVector(corners[n].xz() - xz_origin, angle) + xz_origin, corners[n].y);

	Box<float3> out(corners[0], corners[0]);
	for(int n = 0; n < arraySize(corners); n++) {
		out.min = min(out.min, corners[n]);
		out.max = max(out.max, corners[n]);
	}

	return out;
}

float3 Matrix3::operator*(const float3 &vec) const {
	return float3(dot(vec, v[0]), dot(vec, v[1]), dot(vec, v[2]));
}

Matrix3 Matrix3::identity() { return Matrix3(float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1)); }

Matrix3::Matrix3(const float3 &a, const float3 &b, const float3 &c) {
	v[0] = a;
	v[1] = b;
	v[2] = c;
}

Matrix3 transpose(const Matrix3 &mat) {
	return Matrix3(float3(mat[0].x, mat[1].x, mat[2].x), float3(mat[0].y, mat[1].y, mat[2].y),
				   float3(mat[0].z, mat[1].z, mat[2].z));
}

Matrix3 inverse(const Matrix3 &mat) {
	Matrix3 out;

	out[0].x = mat[1].y * mat[2].z - mat[1].z * mat[2].y;
	out[0].y = mat[0].z * mat[2].y - mat[0].y * mat[2].z;
	out[0].z = mat[0].y * mat[1].z - mat[0].z * mat[1].y;
	out[1].x = mat[1].z * mat[2].x - mat[1].x * mat[2].z;
	out[1].y = mat[0].x * mat[2].z - mat[0].z * mat[2].x;
	out[1].z = mat[0].z * mat[1].x - mat[0].x * mat[1].z;
	out[2].x = mat[1].x * mat[2].y - mat[1].y * mat[2].x;
	out[2].y = mat[0].y * mat[2].x - mat[0].x * mat[2].y;
	out[2].z = mat[0].x * mat[1].y - mat[0].y * mat[1].x;

	float det = mat[0].x * out[0].x + mat[0].y * out[1].x + mat[0].z * out[2].x;
	// TODO what if det close to 0?
	float idet = 1.0f / det;

	return Matrix3(out[0] * idet, out[1] * idet, out[2] * idet);
}

Segment::Segment(const Ray &ray, float min, float max) : Ray(ray), m_min(min), m_max(max) {}

Segment::Segment(const float3 &source, const float3 &target) {
	float3 dir = target - source;
	float len = length(dir);
	*((Ray *)this) = Ray(source, dir / len);
	m_min = 0.0f;
	m_max = len;
}

static bool isNormalized(const float3 &vec) {
	float len = lengthSq(vec);
	return len <= 1.0f + constant::epsilon && len >= 1.0f - constant::epsilon;
}

Plane::Plane(const float3 &normalized_dir, float distance)
	: m_dir(normalized_dir), m_distance(distance) {
	DASSERT(isNormalized(m_dir));
}

Plane::Plane(const float3 &origin, const float3 &normalized_dir)
	: m_dir(normalized_dir), m_distance(dot(origin, normalized_dir)) {
	DASSERT(isNormalized(m_dir));
}

Plane::Plane(const float3 &a, const float3 &b, const float3 &c) {
	m_dir = normalized(cross(c - a, b - a));
	m_distance = dot(a, m_dir);
}

float3 project(const float3 &point, const Plane &plane) {
	float dist = dot(point, plane.normal()) - plane.distance();
	return point - plane.normal() * dist;
}

float intersection(const Ray &ray, const Box<float3> &box) {
	// TODO: check if works correctly for (+/-)INF
	float3 inv_dir = ray.invDir();
	float3 origin = ray.origin();

	float l1 = inv_dir.x * (box.min.x - origin.x);
	float l2 = inv_dir.x * (box.max.x - origin.x);
	float lmin = min(l1, l2);
	float lmax = max(l1, l2);

	l1 = inv_dir.y * (box.min.y - origin.y);
	l2 = inv_dir.y * (box.max.y - origin.y);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);

	l1 = inv_dir.z * (box.min.z - origin.z);
	l2 = inv_dir.z * (box.max.z - origin.z);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);

	return lmin <= lmax ? lmin : constant::inf;
}

float intersection(const Interval idir[3], const Interval origin[3], const Box<float3> &box) {
	Interval l1, l2, lmin, lmax;

	l1 = idir[0] * (Interval(box.min.x) - origin[0]);
	l2 = idir[0] * (Interval(box.max.x) - origin[0]);
	lmin = min(l1, l2);
	lmax = max(l1, l2);

	l1 = idir[1] * (Interval(box.min.y) - origin[1]);
	l2 = idir[1] * (Interval(box.max.y) - origin[1]);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);

	l1 = idir[2] * (Interval(box.min.z) - origin[2]);
	l2 = idir[2] * (Interval(box.max.z) - origin[2]);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);

	return lmin.min <= lmax.max ? lmin.min : constant::inf;
}

float intersection(const Segment &segment, const Box<float3> &box) {
	// TODO: check if works correctly for (+/-)INF
	float3 inv_dir = segment.invDir();
	float3 origin = segment.origin();

	float l1 = inv_dir.x * (box.min.x - origin.x);
	float l2 = inv_dir.x * (box.max.x - origin.x);
	float lmin = min(l1, l2);
	float lmax = max(l1, l2);

	l1 = inv_dir.y * (box.min.y - origin.y);
	l2 = inv_dir.y * (box.max.y - origin.y);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);

	l1 = inv_dir.z * (box.min.z - origin.z);
	l2 = inv_dir.z * (box.max.z - origin.z);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);
	lmin = max(lmin, segment.min());
	lmax = min(lmax, segment.max());

	return lmin <= lmax ? lmin : constant::inf;
}

float dot(const float2 &a, const float2 &b) { return a.x * b.x + a.y * b.y; }

float dot(const float3 &a, const float3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

float dot(const float4 &a, const float4 &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float3 cross(const float3 &a, const float3 &b) {
	return float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float lengthSq(const float2 &v) { return dot(v, v); }
float lengthSq(const float3 &v) { return dot(v, v); }
float distanceSq(const float3 &a, const float3 &b) { return lengthSq(a - b); }
float distanceSq(const float2 &a, const float2 &b) { return lengthSq(a - b); }

float length(const float2 &v) { return sqrtf(lengthSq(v)); }
float length(const float3 &v) { return sqrtf(lengthSq(v)); }
float distance(const float3 &a, const float3 &b) { return sqrtf(distanceSq(a, b)); }
float distance(const float2 &a, const float2 &b) { return sqrtf(distanceSq(a, b)); }

float angleDistance(float a, float b) {
	float diff = fabs(a - b);
	return min(diff, constant::pi * 2.0f - diff);
}

float blendAngles(float initial, float target, float step) {
	if(initial != target) {
		float new_ang1 = initial + step, new_ang2 = initial - step;
		if(new_ang1 < 0.0f)
			new_ang1 += constant::pi * 2.0f;
		if(new_ang2 < 0.0f)
			new_ang2 += constant::pi * 2.0f;
		float new_angle =
			angleDistance(new_ang1, target) < angleDistance(new_ang2, target) ? new_ang1 : new_ang2;
		if(angleDistance(initial, target) < step)
			new_angle = target;
		return new_angle;
	}

	return initial;
}

bool areAdjacent(const IRect &a, const IRect &b) {
	if(b.min.x < a.max.x && a.min.x < b.max.x)
		return a.max.y == b.min.y || a.min.y == b.max.y;
	if(b.min.y < a.max.y && a.min.y < b.max.y)
		return a.max.x == b.min.x || a.min.x == b.max.x;
	return false;
}

float distanceSq(const Rect<float2> &a, const Rect<float2> &b) {
	float2 p1 = clamp(b.center(), a.min, a.max);
	float2 p2 = clamp(p1, b.min, b.max);
	return distanceSq(p1, p2);
}

float distanceSq(const Box<float3> &a, const Box<float3> &b) {
	float3 p1 = clamp(b.center(), a.min, a.max);
	float3 p2 = clamp(p1, b.min, b.max);
	return distanceSq(p1, p2);
}

bool isInsideFrustum(const float3 &eye_pos, const float3 &eye_dir, float min_dot, const FBox &box) {
	float3 corners[8];
	box.getCorners(corners);

	for(int n = 0; n < arraySize(corners); n++) {
		float3 cvector = corners[n] - eye_pos;
		float len = length(cvector);

		if(dot(cvector, eye_dir) >= min_dot * len)
			return true;
	}

	return false;
}
}
