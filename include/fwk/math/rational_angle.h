// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/rational.h"

namespace fwk {

// TODO: można wyznaczyć spread między kątami
// to by nam pewnie pozwoliło napisac funkcje: ccwAngle(v1, v2), cwAngle(v1, v2)

template <class T> struct RationalAngle {
	static_assert(is_vec<T> && !is_rational<T>);

	using Scalar = MakeRat<fwk::Scalar<T>>;

	RationalAngle(T);
	RationalAngle(const Rational<fwk::Scalar<T>> &vec) : RationalAngle(T(vec.num())) {}

	RationalAngle() : slope(0), quadrant(0) {}
	RationalAngle(const Scalar &slope, int quadrant) : slope(slope), quadrant(quadrant) {
		PASSERT(quadrant >= 0 && quadrant <= 3);
	}

	explicit operator double() const;
	void operator>>(TextFormatter &) const;

	// TODO: there are two useful negations:
	// - one which adds 180 degrees
	// - another which works just like normal angle negation (30 deg -> -30 deg)
	RationalAngle operator-() const {
		return {-slope, (quadrant & 1 ? quadrant + 1 : quadrant + 3) & 3};
	}

	FWK_ORDER_BY(RationalAngle, quadrant, slope);

	Scalar slope;
	short quadrant;
};

}
