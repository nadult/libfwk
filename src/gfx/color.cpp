/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"

namespace fwk {

const Color Color::white(255, 255, 255);
const Color Color::gray(127, 127, 127);
const Color Color::yellow(255, 255, 0);
const Color Color::red(255, 0, 0);
const Color Color::green(0, 255, 0);
const Color Color::blue(0, 0, 255);
const Color Color::black(0, 0, 0);
const Color Color::transparent(0, 0, 0, 0);

Color mulAlpha(Color color, float alpha_mul) {
	float4 fcolor(color);
	fcolor.w *= alpha_mul;
	return Color(fcolor);
}

Color lerp(Color a, Color b, float value) { return Color(lerp((float4)a, (float4)b, value)); }

Color desaturate(Color col, float value) {
	float4 rgba(col);
	float avg =
		sqrtf(rgba.x * rgba.x * 0.299f + rgba.y * rgba.y * 0.587f + rgba.z * rgba.z * 0.114f);
	return lerp(col, Color(avg, avg, avg, rgba.w), value);
}
}
