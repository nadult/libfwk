// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/colored_triangle.h"

#include "fwk/math/box.h"
#include "fwk/math/constants.h"
#include "fwk/sys/stream.h"

namespace fwk {

FBox enclose(CSpan<ColoredTriangle> tris) {
	if(tris) {
		float3 min(inf), max(-inf);
		for(auto &tri : tris)
			for(auto pt : tri.points()) {
				min = vmin(min, pt);
				max = vmax(max, pt);
			}
		return {min, max};
	}
	return FBox();
}

ColoredTriangle::ColoredTriangle(float3 a, float3 b, float3 c, IColor col)
	: Triangle3F(a, b, c), colors{{col, col, col}} {}
ColoredTriangle::ColoredTriangle(CSpan<float3, 3> pts, IColor col)
	: ColoredTriangle(pts[0], pts[1], pts[2], col) {}
ColoredTriangle::ColoredTriangle(CSpan<float3, 3> pts, CSpan<IColor, 3> cols)
	: Triangle3F(pts[0], pts[1], pts[2]), colors{{cols[0], cols[1], cols[2]}} {}

ColoredTriangle::ColoredTriangle(Triangle3F tri, IColor col)
	: ColoredTriangle(tri[0], tri[1], tri[2], col) {}

void ColoredTriangle::save(Stream &sr) const {
	sr.pack((*this)[0], (*this)[1], (*this)[2], colors[0], colors[1], colors[2]);
}

void ColoredTriangle::load(Stream &sr) {
	float3 points[3];
	sr.unpack(points[0], points[1], points[2], colors[0], colors[1], colors[2]);
	*this = {points[0], points[1], points[2]};
}

ColoredTriangle ColoredTriangle::flipped() const {
	return {Triangle3F::flipped().points(), {colors[2], colors[1], colors[0]}};
}
}
