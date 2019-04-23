// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/rotation.h"

#ifdef _WIN32
#include <float.h>
#endif
#include "fwk/sys/assert.h"

namespace fwk {

float angleDistance(float a, float b) {
	float diff = fabs(a - b);
	return min(diff, pi * 2.0f - diff);
}

float blendAngles(float initial, float target, float step) {
	if(initial != target) {
		float new_ang1 = initial + step, new_ang2 = initial - step;
		if(new_ang1 < 0.0f)
			new_ang1 += pi * 2.0f;
		if(new_ang2 < 0.0f)
			new_ang2 += pi * 2.0f;
		float new_angle =
			angleDistance(new_ang1, target) < angleDistance(new_ang2, target) ? new_ang1 : new_ang2;
		if(angleDistance(initial, target) < step)
			new_angle = target;
		return new_angle;
	}

	return initial;
}

float normalizeAngle(float angle) {
	angle = fmodf(angle, 2.0f * pi);
	if(angle < 0.0f)
		angle += 2.0f * pi;
	return angle;
}

float vectorToAngle(const float2 &normalized_vec) {
	DASSERT_EX(isNormalized(normalized_vec), normalized_vec);
	float ang = std::acos(normalized_vec.x);
	return normalized_vec.y < 0 ? 2.0f * pi - ang : ang;
}

double vectorToAngle(const double2 &normalized_vec) {
	DASSERT_EX(isNormalized(normalized_vec), normalized_vec);
	double ang = std::acos(normalized_vec.x);
	return normalized_vec.y < 0.0 ? 2.0 * pi - ang : ang;
}

float2 angleToVector(float radians) {
	auto sc = sincos(radians);
	return float2(sc.second, sc.first);
}

double2 angleToVector(double radians) {
	auto sc = sincos(radians);
	return double2(sc.second, sc.first);
}

float2 rotateVector(const float2 &vec, float radians) {
	auto sc = sincos(radians);
	return float2(sc.second * vec.x - sc.first * vec.y, sc.second * vec.y + sc.first * vec.x);
}

double2 rotateVector(const double2 &vec, double radians) {
	auto sc = sincos(radians);
	return double2(sc.second * vec.x - sc.first * vec.y, sc.second * vec.y + sc.first * vec.x);
}

float3 rotateVector(const float3 &pos, const float3 &axis, float radians) {
	auto sc = sincos(radians);
	return pos * sc.second + cross(axis, pos) * sc.first + axis * dot(axis, pos) * (1 - sc.second);
}
double3 rotateVector(const double3 &pos, const double3 &axis, double radians) {
	auto sc = sincos(radians);
	return pos * sc.second + cross(axis, pos) * sc.first + axis * dot(axis, pos) * (1 - sc.second);
}

template <class Vec, class Real = typename Vec::Scalar>
Real angleTowards(const Vec &prev, const Vec &cur, const Vec &next) {
	DASSERT_NE(prev, cur);
	DASSERT_NE(cur, next);
	Vec vec1 = normalize(cur - prev), vec2 = normalize(next - cur);
	return atan2(cross(vec1, vec2), dot(vec1, vec2));
}

template <class Vec, class Real = typename Vec::Scalar>
Real angleBetween(const Vec &vec1, const Vec &vec2) {
	DASSERT_EX(isNormalized(vec1), vec1);
	DASSERT_EX(isNormalized(vec2), vec2);
	auto ang = atan2(cross(vec1, vec2), dot(vec1, vec2));
	if(ang < Real(0))
		ang += pi * Real(2);
	return ang;
}

float angleBetween(const float2 &a, const float2 &b) { return angleBetween<float2>(a, b); }
double angleBetween(const double2 &a, const double2 &b) { return angleBetween<double2>(a, b); }

float angleTowards(const float2 &p, const float2 &c, const float2 &n) {
	return angleTowards<float2>(p, c, n);
}

double angleTowards(const double2 &p, const double2 &c, const double2 &n) {
	return angleTowards<double2>(p, c, n);
}
}
