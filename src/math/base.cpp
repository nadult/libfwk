// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math_base.h"

namespace fwk {

// TODO: tak czy ze wskaznikami?
Pair<float> sincos(float radians) {
	Pair<float> out;
#ifdef FWK_PLATFORM_MSVC
	out.first = ::sinf(radians);
	out.second = ::cosf(radians);
#else
	::sincosf(radians, &out.first, &out.second);
#endif
	return out;
}

Pair<double> sincos(double radians) {
	Pair<double> out;
#ifdef FWK_PLATFORM_MSVC
	out.first = ::sin(radians);
	out.second = ::cos(radians);
#else
	::sincos(radians, &out.first, &out.second);
#endif
	return out;
}

float frand() { return float(rand()) / float(RAND_MAX); }
}
