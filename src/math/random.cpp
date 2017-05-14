/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>
 */

#include "fwk_math.h"
#include <random>

namespace fwk {

Random::Random(RandomSeed seed) { m_engine.seed(seed); }

RandomSeed Random::operator()() { return m_engine(); }

int Random::uniform(int min, int max) {
	DASSERT(max >= min);
	return std::uniform_int_distribution<int>(min, max)(m_engine);
}

float Random::uniform(float min, float max) {
	DASSERT(max >= min);
	return std::uniform_real_distribution<float>(min, max)(m_engine);
}

double Random::uniform(double min, double max) {
	DASSERT(max >= min);
	return std::uniform_real_distribution<double>(min, max)(m_engine);
}

Quat Random::uniformRotation() { return uniformRotation(sampleUnitHemisphere<float3>()); }

Quat Random::uniformRotation(float3 axis) {
	return (Quat)AxisAngle(axis, uniform(0.0f, fconstant::pi * 2.0f));
}
}
