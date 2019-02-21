// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/rational_angle.h"

#include "fwk/format.h"
#include "fwk/math/ext24.h"

namespace fwk {

template <class T> RationalAngle<T>::RationalAngle(T vec) {
	PASSERT(vec != T());

	if constexpr(is_rational<T>) {
		int xsign = vec.numX() == 0 ? 0 : vec.numX() < 0 ? -1 : 1;
		int ysign = vec.numY() == 0 ? 0 : vec.numY() < 0 ? -1 : 1;

		PASSERT(xsign != 0 || ysign != 0);
		quadrant = xsign >= 0 ? ysign < 0 ? 3 : 0 : ysign < 0 ? 2 : 1;

		slope = ratDivide(vec.numY(), vec.numX());

	} else {
		int xsign = vec.x == 0 ? 0 : vec.x < 0 ? -1 : 1;
		int ysign = vec.y == 0 ? 0 : vec.y < 0 ? -1 : 1;

		PASSERT(xsign != 0 || ysign != 0);
		quadrant = xsign >= 0 ? ysign < 0 ? 3 : 0 : ysign < 0 ? 2 : 1;

		slope = ratDivide(vec.y, vec.x);
	}
}

template <class T> RationalAngle<T>::operator double() const {
	auto value = std::atan(double(slope));
	return value + (quadrant == 0 ? 0.0 : quadrant == 3 ? pi * 2.0 : double(pi));
}

template <class T> void RationalAngle<T>::operator>>(TextFormatter &out) const {
	out(out.isStructured() ? "Q:% S:%" : "% %", quadrant, slope);
}

template struct RationalAngle<short2>;
template struct RationalAngle<int2>;
template struct RationalAngle<llint2>;

template struct RationalAngle<vec2<Ext24<short>>>;
template struct RationalAngle<vec2<Ext24<int>>>;
template struct RationalAngle<vec2<Ext24<llint>>>;

template struct RationalAngle<Rat2S>;
template struct RationalAngle<Rat2I>;
template struct RationalAngle<Rat2L>;

template struct RationalAngle<Rat2ES>;
template struct RationalAngle<Rat2EI>;
template struct RationalAngle<Rat2EL>;

}
