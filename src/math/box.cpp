// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/box.h"

#include "fwk/format.h"
#include "fwk/math/matrix4.h"
#include "fwk/math/plane.h"

namespace fwk {

template <class T> void checkBoxRangeDetailed(const T &min, const T &max) {
	if(!validBoxRange(min, max))
		FATAL("Invalid box range: %s", format("% - %", min, max).c_str());
}

void checkBoxRange(const float2 &min, const float2 &max) { return checkBoxRangeDetailed(min, max); }
void checkBoxRange(const float3 &min, const float3 &max) { return checkBoxRangeDetailed(min, max); }
void checkBoxRange(const int2 &min, const int2 &max) { return checkBoxRangeDetailed(min, max); }
void checkBoxRange(const int3 &min, const int3 &max) { return checkBoxRangeDetailed(min, max); }

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

array<Pair<float3>, 12> edges(const FBox &box) {
	array<Pair<float3>, 12> out;
	auto corners = box.corners();
	int indices[12][2] = {{7, 3}, {3, 2}, {2, 6}, {6, 7}, {5, 1}, {1, 0},
						  {0, 4}, {4, 5}, {5, 7}, {1, 3}, {0, 2}, {4, 6}};
	for(int n = 0; n < 12; n++)
		out[n] = {corners[indices[n][0]], corners[indices[n][1]]};
	return out;
}
}
