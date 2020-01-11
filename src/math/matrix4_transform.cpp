// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/matrix4.h"

#include "fwk/math/frustum.h"
#include "fwk/math/plane.h"
#include "fwk/math/segment.h"
#include "fwk/math/triangle.h"

namespace fwk {

Plane3F transform(const Matrix4 &matrix, const Plane3F &plane) {
	float3 v1 = plane.normal();
	int min = v1[0] < v1[1] ? 0 : 1;
	if(v1[2] < v1[min])
		min = 2;
	v1[min] = 2.0f;
	v1 = normalize(v1);
	float3 v2 = cross(plane.normal(), v1);

	float3 p0 = plane.normal() * plane.distance0();
	float3 p1 = p0 + v1, p2 = p0 + v2;
	p1 -= plane.normal() * plane.signedDistance(p1);
	p2 -= plane.normal() * plane.signedDistance(p2);
	// TODO: this can be faster: transform normal directly ?

	return Plane3F(transform(matrix, Triangle3F(p0, p1, p2)));
	// TODO: this is incorrect:
	/*	float3 new_p = mulPoint(m, p.normal() * p.distance());
		float3 new_n = normalize(mulNormal(m, p.normal()));
		return Plane(new_n, dot(new_n, new_p));*/
}

Frustum transform(const Matrix4 &matrix, const Frustum &frustum) {
	Frustum out;
	for(auto pid : all<FrustumPlaneId>)
		out[pid] = transform(matrix, frustum[pid]);
	return out;
}

Triangle3F transform(const Matrix4 &matrix, const Triangle3F &tri) {
	return {mulPoint(matrix, tri[0]), mulPoint(matrix, tri[1]), mulPoint(matrix, tri[2])};
}

Segment<float3> transform(const Matrix4 &matrix, const Segment<float3> &segment) {
	return {mulPoint(matrix, segment.from), mulPoint(matrix, segment.to)};
}
}
