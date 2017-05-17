/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_math.h"

namespace fwk {

float distance(const Cylinder &cyl, const float3 &point) {
	float3 closest = point;
	closest.x = clamp(closest.x, cyl.pos().x - cyl.radius(), cyl.pos().x + cyl.radius());
	closest.y = clamp(closest.y, cyl.pos().y, cyl.pos().y + cyl.height());
	closest.z = clamp(closest.z, cyl.pos().z - cyl.radius(), cyl.pos().z + cyl.radius());

	if(distanceSq(closest.xz(), cyl.pos().xz()) > cyl.radius() * cyl.radius()) {
		float2 vec = normalize(closest.xz() - cyl.pos().xz()) * cyl.radius();
		closest.x = cyl.pos().x + vec[0];
		closest.z = cyl.pos().z + vec[1];
	}

	return distance(closest, point);
}

bool areIntersecting(const Cylinder &lhs, const Cylinder &rhs) {
	if(distance(lhs.pos().xz(), rhs.pos().xz()) > lhs.radius() + rhs.radius())
		return false;
	return lhs.pos().y <= rhs.pos().y + rhs.height() && rhs.pos().y <= lhs.pos().y + lhs.height();
}

bool areIntersecting(const FBox &box, const Cylinder &cylinder) {
	float2 box_point = box.xz().closestPoint(cylinder.pos().xz());
	return distanceSq(box_point, cylinder.pos().xz()) < cylinder.radius() &&
		   box.y() <= cylinder.pos().y + cylinder.height() && cylinder.pos().y <= box.ey();
}
}
