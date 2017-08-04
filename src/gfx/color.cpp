// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_gfx.h"
#include <cstring>

namespace fwk {

FColor::FColor(ColorId color_id) {
	static EnumMap<ColorId, FColor> map = {{
		{ColorId::white, FColor(1.0f, 1.0f, 1.0f)},
		{ColorId::gray, FColor(0.5f, 0.5f, 0.5f)},
		{ColorId::yellow, FColor(1.0f, 1.0f, 0.0f)},
		{ColorId::cyan, FColor(0.0f, 1.0f, 1.0f)},
		{ColorId::magneta, FColor(1.0f, 0.0f, 1.0f)},
		{ColorId::purple, FColor(0.5f, 0.0f, 0.5f)},
		{ColorId::brown, FColor(0.647f, 0.164f, 0.164f)},
		{ColorId::orange, FColor(1.0f, 0.647f, 0.0f)},
		{ColorId::gold, FColor(1.0f, 0.843f, 0.0f)},
		{ColorId::red, FColor(1.0f, 0.0f, 0.0f)},
		{ColorId::green, FColor(0.0f, 1.0f, 0.0f)},
		{ColorId::blue, FColor(0.0f, 0.0f, 1.0f)},
		{ColorId::black, FColor(0.0f, 0.0f, 0.0f)},
		{ColorId::transparent, FColor(0.0f, 0.0f, 0.0f, 0.0f)},
	}};
	*this = map[color_id];
}

FColor mulAlpha(FColor color, float alpha_mul) {
	color.a *= alpha_mul;
	return color;
}

FColor desaturate(FColor col, float value) {
	float avg = sqrtf(col.r * col.r * 0.299f + col.g * col.g * 0.587f + col.b * col.b * 0.114f);
	return lerp(col, FColor(avg, avg, avg, col.a), value);
}

FColor srgbToLinear(const FColor &c) {
	float exp = 2.2;
	return FColor(powf(c.r, exp), powf(c.g, exp), powf(c.b, exp), c.a);
}

FColor linearToSrgb(const FColor &c) {
	float exp = 1.0f / 2.2f;
	return FColor(powf(c.r, exp), powf(c.g, exp), powf(c.b, exp), c.a);
}

// Source: blender
float3 hsvToRgb(float3 hsv) {
	float nr = fabsf(hsv[0] * 6.0f - 3.0f) - 1.0f;
	float ng = 2.0f - fabsf(hsv[0] * 6.0f - 2.0f);
	float nb = 2.0f - fabsf(hsv[0] * 6.0f - 4.0f);

	nr = clamp(nr, 0.0f, 1.0f);
	nb = clamp(nb, 0.0f, 1.0f);
	ng = clamp(ng, 0.0f, 1.0f);

	return float3(((nr - 1.0f) * hsv[1] + 1.0f) * hsv[2], ((ng - 1.0f) * hsv[1] + 1.0f) * hsv[2],
				  ((nb - 1.0f) * hsv[1] + 1.0f) * hsv[2]);
}

float3 rgbToHsv(float3 rgb) {
	float k = 0.0f;

	if(rgb[1] < rgb[2]) {
		swap(rgb[1], rgb[2]);
		k = -1.0f;
	}

	float min_gb = rgb[2];
	if(rgb[0] < rgb[1]) {
		swap(rgb[0], rgb[1]);
		k = -2.0f / 6.0f - k;
		min_gb = min(rgb[1], rgb[2]);
	}

	float chroma = rgb[0] - min_gb;

	return float3(fabsf(k + (rgb[1] - rgb[2]) / (6.0f * chroma + 1e-20f)),
				  chroma / (rgb[0] + 1e-20f), rgb[0]);
}
}
