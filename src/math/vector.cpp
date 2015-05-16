/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include <cmath>
#ifdef _WIN32
#include <float.h>
#endif

#ifdef WIN32
void sincosf(float rad, float *s, float *c) {
	DASSERT(s && c);
	*s = sin(rad);
	*c = cos(rad);
}
#endif

namespace fwk {

const int2 min(const int2 &a, const int2 &b) { return int2(min(a.x, b.x), min(a.y, b.y)); }
const int2 max(const int2 &a, const int2 &b) { return int2(max(a.x, b.x), max(a.y, b.y)); }
const int3 min(const int3 &a, const int3 &b) {
	return int3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}
const int3 max(const int3 &a, const int3 &b) {
	return int3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}

const int2 abs(const int2 &v) { return int2(v.x < 0 ? -v.x : v.x, v.y < 0 ? -v.y : v.y); }
const int3 abs(const int3 &v) {
	return int3(v.x < 0 ? -v.x : v.x, v.y < 0 ? -v.y : v.y, v.z < 0 ? -v.z : v.z);
}

float dot(const float2 &a, const float2 &b) { return a.x * b.x + a.y * b.y; }
float dot(const float3 &a, const float3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float dot(const float4 &a, const float4 &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float lengthSq(const float2 &v) { return dot(v, v); }
float lengthSq(const float3 &v) { return dot(v, v); }
float distanceSq(const float3 &a, const float3 &b) { return lengthSq(a - b); }
float distanceSq(const float2 &a, const float2 &b) { return lengthSq(a - b); }

float length(const float2 &v) { return sqrt(lengthSq(v)); }
float length(const float3 &v) { return sqrt(lengthSq(v)); }
float distance(const float3 &a, const float3 &b) { return sqrt(distanceSq(a, b)); }
float distance(const float2 &a, const float2 &b) { return sqrt(distanceSq(a, b)); }

const float2 normalize(const float2 &v) { return v / length(v); }
const float3 normalize(const float3 &v) { return v / length(v); }

const float3 cross(const float3 &a, const float3 &b) {
	return float3(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
}

const float2 inv(const float2 &v) { return float2(1.0f / v.x, 1.0f / v.y); }
const float3 inv(const float3 &v) { return float3(1.0f / v.x, 1.0f / v.y, 1.0f / v.z); }
const float4 inv(const float4 &v) { return float4(1.0f / v.x, 1.0f / v.y, 1.0f / v.z, 1.0f / v.w); }

float vectorToAngle(const float2 &normalized_vec) {
	float ang = acos(normalized_vec.x);
	return normalized_vec.y < 0 ? 2.0f * constant::pi - ang : ang;
}

const float2 angleToVector(float radians) {
	float s, c;
	sincosf(radians, &s, &c);
	return float2(c, s);
}

const float2 rotateVector(const float2 &vec, float radians) {
	float s, c;
	sincosf(radians, &s, &c);
	return float2(c * vec.x - s * vec.y, c * vec.y + s * vec.x);
}

const float3 rotateVector(const float3 &pos, const float3 &axis, float radians) {
	float s, c;
	sincosf(radians, &s, &c);

	return pos * c + cross(axis, pos) * s + axis * dot(axis, pos) * (1 - c);
}

float triangleArea(const float3 &pa, const float3 &pb, const float3 &pc) {
	return 0.5f * length(cross(pb - pa, pc - pa));
}

bool isnan(float f) {
#ifdef _WIN32
	volatile float vf = f;
	return vf != vf;
#else
	return ::isnan(f);
#endif
}

bool isnan(const float2 &v) { return isnan(v.x) || isnan(v.y); }
bool isnan(const float3 &v) { return isnan(v.x) || isnan(v.y) || isnan(v.z); }
bool isnan(const float4 &v) { return isnan(v.x) || isnan(v.y) || isnan(v.z) || isnan(v.w); }

float frand() { return float(rand()) / float(RAND_MAX); }

}
