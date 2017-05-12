/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

bool isnan(float f) {
#ifdef _WIN32
	volatile float vf = f;
	return vf != vf;
#else
	return std::isnan(f);
#endif
}

bool isnan(const float2 &v) { return isnan(v.x) || isnan(v.y); }
bool isnan(const float3 &v) { return isnan(v.x) || isnan(v.y) || isnan(v.z); }
bool isnan(const float4 &v) { return isnan(v.x) || isnan(v.y) || isnan(v.z) || isnan(v.w); }

float angleDistance(float a, float b) {
	float diff = fabs(a - b);
	return min(diff, fconstant::pi * 2.0f - diff);
}

float blendAngles(float initial, float target, float step) {
	if(initial != target) {
		float new_ang1 = initial + step, new_ang2 = initial - step;
		if(new_ang1 < 0.0f)
			new_ang1 += fconstant::pi * 2.0f;
		if(new_ang2 < 0.0f)
			new_ang2 += fconstant::pi * 2.0f;
		float new_angle =
			angleDistance(new_ang1, target) < angleDistance(new_ang2, target) ? new_ang1 : new_ang2;
		if(angleDistance(initial, target) < step)
			new_angle = target;
		return new_angle;
	}

	return initial;
}

float vectorToAngle(const float2 &vec1, const float2 &vec2) {
	// DASSERT(distance(prev, cur) > fconstant::epsilon && distance(cur, next) >
	// fconstant::epsilon);
	float vcross = -cross(-vec1, vec2);
	float vdot = dot(vec2, vec1);

	float ang = atan2(vcross, vdot);
	if(ang < 0.0f)
		ang = fconstant::pi * 2.0f + ang;
	DASSERT(!isnan(ang));
	return ang;
}

template <class Vec, class Real = typename Vec::Scalar>
Real angleBetween(const Vec &prev, const Vec &cur, const Vec &next) {
	// DASSERT(distance(prev, cur) > fconstant::epsilon && distance(cur, next) >
	// fconstant::epsilon);
	Real vcross = -cross(normalize(cur - prev), normalize(next - cur));
	Real vdot = dot(normalize(next - cur), normalize(prev - cur));

	Real ang = atan2(vcross, vdot);
	if(ang < 0.0f)
		ang = constant<Real>::pi * 2.0 + ang;
	DASSERT(!isnan(ang));
	return ang;
}

float angleBetween(const float2 &prev, const float2 &cur, const float2 &next) {
	return angleBetween<float2>(prev, cur, next);
}

double angleBetween(const double2 &prev, const double2 &cur, const double2 &next) {
	return 0.0f; // angleBetween<double2>(prev, cur, next);
}

float fixAngle(float angle) {
	angle = fmodf(angle, 2.0f * fconstant::pi);
	if(angle < 0.0f)
		angle += 2.0f * fconstant::pi;
	return angle;
}

float frand() { return float(rand()) / float(RAND_MAX); }
}
