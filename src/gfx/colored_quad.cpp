// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/colored_quad.h"

#include "fwk/format.h"
#include "fwk/gfx/colored_triangle.h"
#include "fwk/math/box.h"
#include "fwk/math/constants.h"
#include "fwk/math/matrix4.h"

namespace fwk {

ColoredQuad::ColoredQuad(float3 a, float3 b, float3 c, float3 d, IColor col)
	: points{a, b, c, d}, color(col) {}
ColoredQuad::ColoredQuad(CSpan<float3, 4> pts, IColor col)
	: ColoredQuad(pts[0], pts[1], pts[2], pts[3], col) {}

Pair<ColoredTriangle> ColoredQuad::tris() const {
	return {{points[0], points[1], points[2], color}, {points[0], points[2], points[3], color}};
}

ColoredQuad ColoredQuad::flipped() const {
	return {points[3], points[2], points[1], points[0], color};
}

void ColoredQuad::operator>>(TextFormatter &out) const {
	out(out.isStructured() ? "(% % % %: %)" : "% % % % %", points[0], points[1], points[2],
		points[3], color);
}

FBox enclose(const ColoredQuad &quad) { return enclose(quad.points); }

FBox enclose(CSpan<ColoredQuad> quads) {
	if(quads) {
		float3 min(inf), max(-inf);
		for(auto &quad : quads)
			for(auto pt : quad.points) {
				min = vmin(min, pt);
				max = vmax(max, pt);
			}
		return {min, max};
	}
	return FBox();
}

vector<ColoredQuad> transform(Matrix4 matrix, vector<ColoredQuad> quads) {
	for(auto &quad : quads)
		for(auto &pt : quad.points)
			pt = mulPoint(matrix, pt);
	return quads;
}

vector<ColoredQuad> setColor(vector<ColoredQuad> quads, IColor color) {
	for(auto &quad : quads)
		quad.color = color;
	return quads;
}

vector<ColoredTriangle> tris(CSpan<ColoredQuad> quads) {
	vector<ColoredTriangle> tris;
	tris.reserve(quads.size() * 2);
	for(auto quad : quads) {
		auto [t1, t2] = quad.tris();
		tris.emplace_back(t1);
		tris.emplace_back(t2);
	}
	return tris;
}
}
