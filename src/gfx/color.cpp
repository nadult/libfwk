// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/enum_map.h"
#include "fwk/format.h"
#include "fwk/gfx/color.h"
#include "fwk/parse.h"

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

float srgbToLinear(float v) {
	return v < 0.04045 ? (1.0 / 12.92) * v : pow((v + 0.055) * (1.0 / 1.055), 2.4);
}

float linearToSrgb(float v) {
	return v < 0.0031308 ? 12.92 * v : 1.055 * pow(v, 1.0 / 2.4) - 0.055;
}

float3 srgbToLinear(float3 v) { return {srgbToLinear(v.x), srgbToLinear(v.y), srgbToLinear(v.z)}; }
float3 linearToSrgb(float3 v) { return {linearToSrgb(v.x), linearToSrgb(v.y), linearToSrgb(v.z)}; }

FColor srgbToLinear(const FColor &c) {
	return {srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b), c.a};
}

FColor linearToSrgb(const FColor &c) {
	return {linearToSrgb(c.r), linearToSrgb(c.g), linearToSrgb(c.b), c.a};
}

FColor mulAlpha(FColor color, float alpha_mul) {
	color.a *= alpha_mul;
	return color;
}

FColor desaturate(FColor col, float value) {
	float avg = std::sqrt(col.r * col.r * 0.299f + col.g * col.g * 0.587f + col.b * col.b * 0.114f);
	return lerp(col, FColor(avg, avg, avg, col.a), value);
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

	return float3(fwk::abs(k + (rgb[1] - rgb[2]) / (6.0f * chroma + 1e-20f)),
				  chroma / (rgb[0] + 1e-20f), rgb[0]);
}

FColor gradientLerp(CSpan<FColor> colors, CSpan<float> values, float value) {
	PASSERT(colors.size() == values.size());

	for(int n = 0; n < values.size(); n++)
		if(values[n] >= value) {
			if(n == 0)
				return colors[0];
			float pos = (value - values[n - 1]) / (values[n] - values[n - 1]);
			return lerp(colors[n - 1], colors[n], pos);
		}
	return colors.back();
}

FColor gradientLerp(CSpan<FColor> colors, float value) {
	PASSERT(colors.size() >= 1);

	value *= colors.size() - 1;
	auto ivalue = int(value);
	float t = value - ivalue;

	return ivalue >= colors.size() - 1 ? colors[ivalue]
									   : lerp(colors[ivalue], colors[ivalue + 1], t);
}

void FColor::operator>>(TextFormatter &fmt) const { fmt << float4(*this); }
void IColor::operator>>(TextFormatter &fmt) const { fmt << int4(*this); }

TextParser &operator>>(TextParser &parser, FColor &col) {
	col = parser.parse<float4>();
	return parser;
}

TextParser &operator>>(TextParser &parser, IColor &col) {
	col = parser.parse<int4>();
	return parser;
}
}
