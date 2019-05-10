// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/colored_triangle.h"

#include "fwk/format.h"
#include "fwk/math/box.h"
#include "fwk/math/constants.h"
#include "fwk/math/matrix4.h"

namespace fwk {

ColoredTriangle::ColoredTriangle(float3 a, float3 b, float3 c, IColor col)
	: Triangle3F(a, b, c), colors{{col, col, col}} {}
ColoredTriangle::ColoredTriangle(CSpan<float3, 3> pts, IColor col)
	: ColoredTriangle(pts[0], pts[1], pts[2], col) {}
ColoredTriangle::ColoredTriangle(CSpan<float3, 3> pts, CSpan<IColor, 3> cols)
	: Triangle3F(pts[0], pts[1], pts[2]), colors{{cols[0], cols[1], cols[2]}} {}

ColoredTriangle::ColoredTriangle(Triangle3F tri, IColor col)
	: ColoredTriangle(tri[0], tri[1], tri[2], col) {}

ColoredTriangle ColoredTriangle::flipped() const {
	return {Triangle3F::flipped().points(), {colors[2], colors[1], colors[0]}};
}

void ColoredTriangle::operator>>(TextFormatter &out) const {
	if(colors[0] == colors[1] && colors[0] == colors[2])
		out(out.isStructured() ? "(%: %)" : "% %", (const Triangle3F &)*this, colors[0]);
	else
		out(out.isStructured() ? "(%: % % %)" : "% % % %", (const Triangle3F &)*this, colors[0],
			colors[1], colors[2]);
}

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

vector<ColoredTriangle> transform(Matrix4 matrix, vector<ColoredTriangle> tris) {
	for(auto &tri : tris)
		tri = {{mulPoint(matrix, tri[0]), mulPoint(matrix, tri[1]), mulPoint(matrix, tri[2])},
			   tri.colors};
	return tris;
}

vector<ColoredTriangle> setColor(vector<ColoredTriangle> tris, IColor color) {
	for(auto &tri : tris)
		tri.setColor(color);
	return tris;
}

}
