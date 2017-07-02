// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_math.h"

#include <cmath>
#ifdef _WIN32
#include <float.h>
#endif
#include "fwk_assert.h"

#if defined(FWK_TARGET_MSVC) || defined(FWK_TARGET_MINGW)
void sincosf(float rad, float *s, float *c) {
	DASSERT(s && c);
	*s = sin(rad);
	*c = cos(rad);
}

void sincos(double rad, double *s, double *c) {
	DASSERT(s && c);
	*s = sin(rad);
	*c = cos(rad);
}
#endif

namespace fwk {

float vectorToAngle(const float2 &normalized_vec) {
	DASSERT(isNormalized(normalized_vec));
	float ang = std::acos(normalized_vec.x);
	return normalized_vec.y < 0 ? 2.0f * fconstant::pi - ang : ang;
}

double vectorToAngle(const double2 &normalized_vec) {
	DASSERT(isNormalized(normalized_vec));
	double ang = std::acos(normalized_vec.x);
	return normalized_vec.y < 0.0 ? 2.0 * dconstant::pi - ang : ang;
}

float2 angleToVector(float radians) {
	float s, c;
	sincosf(radians, &s, &c);
	return float2(c, s);
}

double2 angleToVector(double radians) {
	double s, c;
	sincos(radians, &s, &c);
	return double2(c, s);
}

float2 rotateVector(const float2 &vec, float radians) {
	float s, c;
	sincosf(radians, &s, &c);
	return float2(c * vec.x - s * vec.y, c * vec.y + s * vec.x);
}

double2 rotateVector(const double2 &vec, double radians) {
	double s, c;
	sincos(radians, &s, &c);
	return double2(c * vec.x - s * vec.y, c * vec.y + s * vec.x);
}

float3 rotateVector(const float3 &pos, const float3 &axis, float radians) {
	float s, c;
	sincosf(radians, &s, &c);
	return pos * c + cross(axis, pos) * s + axis * dot(axis, pos) * (1 - c);
}
double3 rotateVector(const double3 &pos, const double3 &axis, double radians) {
	double s, c;
	sincos(radians, &s, &c);
	return pos * c + cross(axis, pos) * s + axis * dot(axis, pos) * (1 - c);
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
	DASSERT_HINT(isNormalized(vec1), vec1);
	DASSERT_HINT(isNormalized(vec2), vec2);
	auto ang = atan2(cross(vec1, vec2), dot(vec1, vec2));
	if(ang < Real(0))
		ang += constant<Real>::pi * Real(2);
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
