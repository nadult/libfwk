/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

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

float angleBetween(const float2 &prev, const float2 &cur, const float2 &next) {
	DASSERT(distance(prev, cur) > constant::epsilon && distance(cur, next) > constant::epsilon);
	float vcross = -cross(normalize(cur - prev), normalize(next - cur));
	float vdot = dot(normalize(next - cur), normalize(prev - cur));

	float ang = atan2(vcross, vdot);
	if(ang < 0.0f)
		ang = constant::pi * 2.0f + ang;
	DASSERT(!isnan(ang));
	return ang;
}

float fixAngle(float angle) {
	angle = fmodf(angle, 2.0f * constant::pi);
	if(angle < 0.0f)
		angle += 2.0f * constant::pi;
	return angle;
}

float frand() { return float(rand()) / float(RAND_MAX); }
}
