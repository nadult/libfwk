#pragma once

#include "fwk/math_base.h"

namespace fwk {

// mu: interpolation parameter between y1 & y2 (0 - 1)
template <class T> T interpCubic(const T &y0, const T &y1, const T &y2, const T &y3, double mu) {
	// Source: http://paulbourke.net/miscellaneous/

	PASSERT(mu >= 0.0 && mu <= 1.0);
	auto mu_sq = mu * mu;
	auto a0 = y3 - y2 - y0 + y1;
	auto a1 = y0 - y1 - a0;
	auto a2 = y2 - y0;
	auto a3 = y1;

	return a0 * mu * mu_sq + a1 * mu_sq + a2 * mu + a3;
}

// mu: interpolation parameter between y1 & y2 (0 - 1)
template <class T>
T interpCatmullRom(const T &y0, const T &y1, const T &y2, const T &y3, double mu) {
	// Source: http://paulbourke.net/miscellaneous/
	PASSERT(mu >= 0.0 && mu <= 1.0);
	auto mu_sq = mu * mu;

	auto a0 = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
	auto a1 = y0 - 2.5 * y1 + 2 * y2 - 0.5 * y3;
	auto a2 = -0.5 * y0 + 0.5 * y2;
	auto a3 = y1;

	return a0 * mu * mu_sq + a1 * mu_sq + a2 * mu + a3;
}
}
