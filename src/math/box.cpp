/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include "fwk_xml.h"

namespace fwk {

template <class TVec3> void Box<TVec3>::getCorners(Range<Vec3, 8> out) const {
	for(int n = 0; n < 8; n++) {
		out[n].x = (n & 4 ? min : max).x;
		out[n].y = (n & 2 ? min : max).y;
		out[n].z = (n & 1 ? min : max).z;
	}
}

template <class TVec3> Box<TVec3>::Box(CRange<Vec3> points) {
	if(points.empty())
		return;

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

	float3 omin = mulPoint(mat, corners[0]), omax = omin;
	for(int n = 1; n < arraySize(corners); n++) {
		float3 point = mulPoint(mat, corners[n]);
		omin = min(omin, point);
		omax = max(omax, point);
	}

	return FBox(omin, omax);
}

bool areOverlapping(const FBox &a, const FBox &b) {
	// TODO: these epsilons shouldnt be here...
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

array<Plane, 6> planes(const FBox &box) {
	array<Plane, 6> out;
	out[0] = Plane({-1, 0, 0}, -box.min.x);
	out[1] = Plane({1, 0, 0}, box.max.x);
	out[2] = Plane({0, -1, 0}, -box.min.y);
	out[3] = Plane({0, 1, 0}, box.max.y);
	out[4] = Plane({0, 0, -1}, -box.min.z);
	out[5] = Plane({0, 0, 1}, box.max.z);
	return out;
}

array<float3, 8> verts(const FBox &box) {
	array<float3, 8> out;
	box.getCorners(out);
	return out;
}

array<pair<float3, float3>, 12> edges(const FBox &box) {
	array<pair<float3, float3>, 12> out;
	float3 corners[8];
	box.getCorners(corners);
	int indices[12][2] = {{7, 3},
						  {3, 2},
						  {2, 6},
						  {6, 7},
						  {5, 1},
						  {1, 0},
						  {0, 4},
						  {4, 5},
						  {5, 7},
						  {1, 3},
						  {0, 2},
						  {4, 6}};
	for(int n = 0; n < 12; n++)
		out[n] = make_pair(corners[indices[n][0]], corners[indices[n][1]]);
	return out;
}

float distance(const Box<float3> &a, const Box<float3> &b) {
	float3 p1 = clamp(b.center(), a.min, a.max);
	float3 p2 = clamp(p1, b.min, b.max);
	return distance(p1, p2);
}

template struct Box<int3>;
template struct Box<float3>;
}
