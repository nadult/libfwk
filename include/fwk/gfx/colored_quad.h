// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/math/box.h"
#include "fwk/math/triangle.h"

namespace fwk {

struct ColoredQuad {
	ColoredQuad() = default;
	ColoredQuad(float3 a, float3 b, float3 c, float3 d, IColor = ColorId::white);
	ColoredQuad(CSpan<float3, 4>, IColor = ColorId::white);

	auto &operator[](int idx) { return points[idx]; }
	const auto &operator[](int idx) const { return points[idx]; }

	Pair<ColoredTriangle> tris() const;
	ColoredQuad flipped() const;

	void operator>>(TextFormatter &) const;

	array<float3, 4> points;
	IColor color;
};

FBox enclose(const ColoredQuad &);
FBox enclose(CSpan<ColoredQuad>);

vector<ColoredQuad> transform(Matrix4, vector<ColoredQuad>);
vector<ColoredQuad> setColor(vector<ColoredQuad>, IColor);

}
