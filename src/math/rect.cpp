/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

template <class TVec2> Rect<TVec2>::Rect(CRange<Vec2> range) { *this = vecMinMax(range); }

bool areAdjacent(const IRect &a, const IRect &b) {
	if(b.min.x < a.max.x && a.min.x < b.max.x)
		return a.max.y == b.min.y || a.min.y == b.max.y;
	if(b.min.y < a.max.y && a.min.y < b.max.y)
		return a.max.x == b.min.x || a.min.x == b.max.x;
	return false;
}

float distanceSq(const FRect &a, const FRect &b) {
	float2 p1 = vclamp(b.center(), a.min, a.max);
	float2 p2 = vclamp(p1, b.min, b.max);
	return distanceSq(p1, p2);
}

bool areOverlapping(const IRect &a, const IRect &b) {
	for(int n = 0; n < 2; n++)
		if(b.min[n] >= a.max[n] || a.min[n] >= b.max[n])
			return false;
	return true;
}

bool areOverlapping(const FRect &a, const FRect &b) {
	for(int n = 0; n < 2; n++)
		if(b.min[n] >= a.max[n] - fconstant::epsilon || a.min[n] >= b.max[n] - fconstant::epsilon)
			return false;
	return true;
}

const Rect<int2> enclosingIRect(const Rect<float2> &frect) {
	return Rect<int2>(floorf(frect.min.x), floorf(frect.min.y), ceilf(frect.max.x),
					  ceilf(frect.max.y));
};

template struct Rect<int2>;
template struct Rect<float2>;
}
