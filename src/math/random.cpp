// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_math.h"

#include <random>

namespace fwk {

// TODO: use Mersenne twister ?
struct Random::Engine : public std::default_random_engine {};

Random::Random(RandomSeed seed) { m_engine->seed(seed); }

Random::Random(const Random &) = default;
Random::Random(Random &&) = default;
Random::~Random() = default;

Random &Random::operator=(const Random &) = default;
Random &Random::operator=(Random &&) = default;

RandomSeed Random::operator()() { return (*m_engine)(); }

int Random::uniform(int min, int max) {
	DASSERT(max >= min);
	return std::uniform_int_distribution<int>(min, max)(*m_engine);
}

long long Random::uniform(long long min, long long max) {
	DASSERT(max >= min);
	return std::uniform_int_distribution<long long>(min, max)(*m_engine);
}

float Random::uniform(float min, float max) {
	DASSERT(max >= min);
	return std::uniform_real_distribution<float>(min, max)(*m_engine);
}

double Random::uniform(double min, double max) {
	DASSERT(max >= min);
	return std::uniform_real_distribution<double>(min, max)(*m_engine);
}

Quat Random::uniformRotation() { return uniformRotation(sampleUnitHemisphere<float3>()); }

Quat Random::uniformRotation(float3 axis) {
	return (Quat)AxisAngle(axis, uniform(0.0f, fconstant::pi * 2.0f));
}
}
