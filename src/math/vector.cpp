/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include <cmath>
#ifdef _WIN32
#include <float.h>
#endif

#if defined(FWK_TARGET_MSVC) || defined(FWK_TARGET_MINGW)
void sincosf(float rad, float *s, float *c) {
	DASSERT(s && c);
	*s = sin(rad);
	*c = cos(rad);
}
#endif

namespace fwk {

float vectorToAngle(const float2 &normalized_vec) {
	DASSERT(isNormalized(normalized_vec));
	float ang = acos(normalized_vec.x);
	return normalized_vec.y < 0 ? 2.0f * fconstant::pi - ang : ang;
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
}
