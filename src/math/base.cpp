// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math_base.h"

namespace fwk {

// TODO: tak czy ze wskaznikami?
pair<float, float> sincos(float radians) {
	pair<float, float> out;
	::sincosf(radians, &out.first, &out.second);
	return out;
}

pair<double, double> sincos(double radians) {
	pair<double, double> out;
	::sincos(radians, &out.first, &out.second);
	return out;
}

float frand() { return float(rand()) / float(RAND_MAX); }
}
