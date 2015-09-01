/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include <cstring>

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

bool Color::operator<(const Color &rhs) const {
	return memcmp(rgba, rhs.rgba, arraySize(rgba)) < 0;		
}

Color operator*(Color a, Color b) {
	float4 fa(a), fb(b);
	return Color(float4(fa.x * fb.x, fa.y * fb.y, fa.z * fb.z, fa.w * fb.w));
}

float3 SRGBToLinear(const float3&v) {
	float exp = 2.2;
	return float3(powf(v.x, exp), powf(v.y, exp), powf(v.z, exp));
}

float3 linearToSRGB(const float3&v) {
	float exp = 1.0f / 2.2f;
	return float3(powf(v.x, exp), powf(v.y, exp), powf(v.z, exp));
}

}
