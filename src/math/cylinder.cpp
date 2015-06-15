/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_math.h"

namespace fwk {

bool areIntersecting(const Cylinder &lhs, const Cylinder &rhs) {
	if(distance(lhs.pos().xz(), rhs.pos().xz()) > lhs.radius() + rhs.radius())
		return false;
	return lhs.pos().y <= rhs.pos().y + rhs.height() && rhs.pos().y <= lhs.pos().y + lhs.height();
}

bool areIntersecting(const FBox &box, const Cylinder &cylinder) {
	float2 box_point = cylinder.pos().xz();
	box_point = max(box_point, box.min.xz());
	box_point = min(box_point, box.max.xz());
	return distanceSq(box_point, cylinder.pos().xz()) < cylinder.radius() &&
		   box.min.y <= cylinder.pos().y + cylinder.height() && cylinder.pos().y <= box.max.y;
}
}