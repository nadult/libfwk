/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include <cstring>

namespace fwk {

FColor::FColor(ColorId color_id) {
	static EnumMap<ColorId, float4> map = {{
		{ColorId::white, FColor(1.0f, 1.0f, 1.0f)},
		{ColorId::gray, FColor(0.5f, 0.5f, 0.5f)},
		{ColorId::yellow, FColor(1.0f, 1.0f, 0)},
		{ColorId::cyan, FColor(0, 1.0f, 1.0f)},
		{ColorId::magneta, FColor(1.0f, 0, 1.0f)},
		{ColorId::purple, FColor(0.5f, 0, 0.5f)},
		{ColorId::brown, FColor(0.647f, 0.164f, 0.164f)},
		{ColorId::orange, FColor(1.0f, 0xA5, 0)},
		{ColorId::gold, FColor(1.0f, 0.843f, 0)},
		{ColorId::red, FColor(1.0f, 0, 0)},
		{ColorId::green, FColor(0, 1.0f, 0)},
		{ColorId::blue, FColor(0, 0, 1.0f)},
		{ColorId::black, FColor(0, 0, 0)},
		{ColorId::transparent, FColor(0, 0, 0, 0)},
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

FColor SRGBToLinear(const FColor &c) {
	float exp = 2.2;
	return FColor(powf(c.r, exp), powf(c.g, exp), powf(c.b, exp), c.a);
}

FColor linearToSRGB(const FColor &c) {
	float exp = 1.0f / 2.2f;
	return FColor(powf(c.r, exp), powf(c.g, exp), powf(c.b, exp), c.a);
}
}
