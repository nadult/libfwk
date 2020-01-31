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

// Spline moves through p0 & p1;
// Normals in p0 and p1 are equal to normalized (p1 - p0) and (p2 - p3) respectively.
template <class T> T cubicBezier(const T &p0, const T &p1, const T &p2, const T &p3, double mu) {
	PASSERT(mu >= 0.0 && mu <= 1.0);

	auto nu = 1.0 - mu;
	auto mu2 = mu * mu;
	auto nu2 = nu * nu;

	return p0 * (nu * nu2) + p1 * (3.0 * mu * nu2) + p2 * (3.0 * mu2 * nu) + p3 * (mu * mu2);
}
}
