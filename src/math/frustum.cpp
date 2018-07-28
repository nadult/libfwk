// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/frustum.h"

#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"

namespace fwk {

static Plane3F makePlane(const float4 &vec) {
	float mul = 1.0f / length(vec.xyz());
	return Plane3F(vec.xyz() * mul, -vec.w * mul);
}

Frustum::Frustum(const Matrix4 &view_proj) {
	Matrix4 t = transpose(view_proj);

	planes[PlaneId::left] = makePlane(t[3] + t[0]);
	planes[PlaneId::right] = makePlane(t[3] - t[0]);

	planes[PlaneId::up] = makePlane(t[3] - t[1]);
	planes[PlaneId::down] = makePlane(t[3] + t[1]);

	//	planes[PlaneId::front]	= makePlane(t[3] - t[2]);
	//	planes[PlaneId::back]	= makePlane(t[3] + t[2]);
}

Frustum::Frustum(CSpan<Plane3F, plane_count> planes) : planes(planes) {}

bool Frustum::testIsect(const float3 &point) const {
	for(auto &plane : planes)
		if(plane.signedDistance(point) <= 0.0f) //TODO: < ?
			return false;
	return true;
}

// TODO: clang crashes on this (in full opt with -DNDEBUG)
bool Frustum::testIsect(CSpan<float3> points) const {
	for(auto &plane : planes) {
		bool all_outside = true;
		for(auto &point : points)
			if(plane.signedDistance(point) > 0.0f) {
				all_outside = false;
				break;
			}
		if(all_outside)
			return false;
	}
	return true;
}

bool Frustum::testIsect(const FBox &box) const { return testIsect(box.corners()); }

const Frustum operator*(const Matrix4 &matrix, const Frustum &frustum) {
	Frustum out;
	for(auto pid : all<FrustumPlaneId>())
		out[pid] = matrix * frustum[pid];
	return out;
}
}
