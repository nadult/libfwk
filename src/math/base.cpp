// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math_base.h"

namespace fwk {

bool isnan(float s) {
#ifdef _WIN32
	volatile float vs = s;
	return vs != vs;
#else
	return std::isnan(s);
#endif
}

bool isnan(double s) {
#ifdef _WIN32
	volatile double vs = s;
	return vs != vs;
#else
	return std::isnan(s);
#endif
}

// TODO: tak czy ze wskaznikami?
pair<float, float> sincos(float radians) {
	pair<float, float> out;
#if defined(FWK_TARGET_MSVC)
	out.first = ::sinf(radians);
	out.second = ::cosf(radians);
#else
	::sincosf(radians, &out.first, &out.second);
#endif
	return out;
}

pair<double, double> sincos(double radians) {
	pair<double, double> out;
#if defined(FWK_TARGET_MSVC)
	out.first = ::sin(radians);
	out.second = ::cos(radians);
#else
	::sincos(radians, &out.first, &out.second);
#endif
	return out;
}

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

float normalizeAngle(float angle) {
	angle = fmodf(angle, 2.0f * fconstant::pi);
	if(angle < 0.0f)
		angle += 2.0f * fconstant::pi;
	return angle;
}

float frand() { return float(rand()) / float(RAND_MAX); }
}
