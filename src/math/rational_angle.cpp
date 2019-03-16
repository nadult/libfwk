// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/rational_angle.h"

#include "fwk/format.h"
#include "fwk/math/direction.h"

namespace fwk {

template <class T> RationalAngle<T>::RationalAngle(T vec) : quadrant(fwk::quadrant(vec)) {
	PASSERT(vec != T());
	slope = ratDivide(vec.y, vec.x);
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

//template struct RationalAngle<vec2<Ext24<short>>>;
//template struct RationalAngle<vec2<Ext24<int>>>;
//template struct RationalAngle<vec2<Ext24<llint>>>;

}
