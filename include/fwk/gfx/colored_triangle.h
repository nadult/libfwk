// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/math/box.h"
#include "fwk/math/triangle.h"

namespace fwk {

struct ColoredTriangle : public Triangle3F {
	ColoredTriangle() = default;
	ColoredTriangle(float3 a, float3 b, float3 c, IColor = ColorId::white);
	ColoredTriangle(CSpan<float3, 3>, IColor = ColorId::white);
	ColoredTriangle(Triangle3F, IColor = ColorId::white);
	ColoredTriangle(CSpan<float3, 3>, CSpan<IColor, 3>);

	ColoredTriangle flipped() const;
	void setColor(IColor col) { colors[0] = colors[1] = colors[2] = col; }

	void operator>>(TextFormatter &) const;

	array<IColor, 3> colors;
};

FBox enclose(CSpan<ColoredTriangle>);
}
