/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

template <class TVec2> Rect<TVec2>::Rect(CRange<Vec2> range) {
	if(range.empty())
		return;

	min = max = range[0];
	for(int n = 1; n < range.size(); n++) {
		min = fwk::min(min, range[n]);
		max = fwk::max(max, range[n]);
	}
}

template <class TVec2> void Rect<TVec2>::getCorners(Range<Vec2, 4> corners) const {
	corners[0] = min;
	corners[1] = Vec2(min.x, max.y);
	corners[2] = max;
	corners[3] = Vec2(max.x, min.y);
}

bool areAdjacent(const IRect &a, const IRect &b) {
	if(b.min.x < a.max.x && a.min.x < b.max.x)
		return a.max.y == b.min.y || a.min.y == b.max.y;
	if(b.min.y < a.max.y && a.min.y < b.max.y)
		return a.max.x == b.min.x || a.min.x == b.max.x;
	return false;
}

float distanceSq(const FRect &a, const FRect &b) {
	float2 p1 = clamp(b.center(), a.min, a.max);
	float2 p2 = clamp(p1, b.min, b.max);
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
		if(b.min[n] >= a.max[n] - constant::epsilon || a.min[n] >= b.max[n] - constant::epsilon)
			return false;
	return true;
}

template struct Rect<int2>;
template struct Rect<float2>;
}
