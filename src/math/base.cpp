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

float frand() { return float(rand()) / float(RAND_MAX); }
}
