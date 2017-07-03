// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_math.h"

namespace fwk {

FBox encloseTransformed(const FBox &box, const Matrix4 &mat) {
	return enclose(transform(box.corners(), [&](auto pt) { return mulPoint(mat, pt); }));
}

array<Plane3F, 6> planes(const FBox &box) {
	array<Plane3F, 6> out;
	out[0] = {{-1, 0, 0}, -box.x()};
	out[1] = {{1, 0, 0}, box.ex()};
	out[2] = {{0, -1, 0}, -box.y()};
	out[3] = {{0, 1, 0}, box.ey()};
	out[4] = {{0, 0, -1}, -box.z()};
	out[5] = {{0, 0, 1}, box.ez()};
	return out;
}

array<float3, 8> verts(const FBox &box) { return box.corners(); }

array<pair<float3, float3>, 12> edges(const FBox &box) {
	array<pair<float3, float3>, 12> out;
	auto corners = box.corners();
	int indices[12][2] = {{7, 3}, {3, 2}, {2, 6}, {6, 7}, {5, 1}, {1, 0},
						  {0, 4}, {4, 5}, {5, 7}, {1, 3}, {0, 2}, {4, 6}};
	for(int n = 0; n < 12; n++)
		out[n] = make_pair(corners[indices[n][0]], corners[indices[n][1]]);
	return out;
}
}
