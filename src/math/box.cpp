/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

template <class TVec3> void Box<TVec3>::getCorners(TRange<Vec3, 8> out) const {
	for(int n = 0; n < 8; n++) {
		out[n].x = (n & 4 ? min : max).x;
		out[n].y = (n & 2 ? min : max).y;
		out[n].z = (n & 1 ? min : max).z;
	}
}

template <class TVec3> Box<TVec3>::Box(CRange<Vec3> points) {
	if(points.empty()) {
		*this = empty();
		return;
	}

	min = max = points[0];
	for(int n = 1; n < points.size(); n++) {
		min = fwk::min(min, points[n]);
		max = fwk::max(max, points[n]);
	}
}

const Box<int3> enclosingIBox(const Box<float3> &fbox) {
	return Box<int3>(floorf(fbox.min.x), floorf(fbox.min.y), floorf(fbox.min.z), ceilf(fbox.max.x),
					 ceilf(fbox.max.y), ceilf(fbox.max.z));
};

const Box<float3> rotateY(const Box<float3> &box, const float3 &origin, float angle) {
	float3 corners[8];
	box.getCorners(corners);
	float2 xz_origin = origin.xz();

	for(int n = 0; n < arraySize(corners); n++)
		corners[n] =
			asXZY(rotateVector(corners[n].xz() - xz_origin, angle) + xz_origin, corners[n].y);

	Box<float3> out(corners[0], corners[0]);
	for(int n = 0; n < arraySize(corners); n++) {
		out.min = min(out.min, corners[n]);
		out.max = max(out.max, corners[n]);
	}

	return out;
}

const FBox operator*(const Matrix4 &mat, const FBox &box) {
	float3 corners[8];
	box.getCorners(corners);

	FBox out;
	out.min = out.max = mulPoint(mat, corners[0]);
	for(int n = 1; n < arraySize(corners); n++)
		out.include(mulPoint(mat, corners[n]));
	return out;
}

bool areOverlapping(const FBox &a, const FBox &b) {
	for(int n = 0; n < 3; n++)
		if(b.min[n] >= a.max[n] - constant::epsilon || a.min[n] >= b.max[n] - constant::epsilon)
			return false;
	return true;
}

bool areOverlapping(const IBox &a, const IBox &b) {
	for(int n = 0; n < 3; n++)
		if(b.min[n] >= a.max[n] || a.min[n] >= b.max[n])
			return false;
	return true;
}

template struct Box<int3>;
template struct Box<float3>;
}
