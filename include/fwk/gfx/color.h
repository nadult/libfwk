// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(ColorId, white, gray, yellow, cyan, magneta, purple, brown, orange, gold, red, green,
			blue, black, transparent);

// 128-bit float-based RGBA color
struct FColor {
	FColor() : r(0.0), g(0.0), b(0.0), a(1.0) {}
	FColor(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
	FColor(const float4 &rgba) : r(rgba[0]), g(rgba[1]), b(rgba[2]), a(rgba[3]) {}
	FColor(const FColor &col, float a) : r(col.r), g(col.g), b(col.b), a(a) {}
	FColor(const float3 &rgb, float a = 1.0) : r(rgb[0]), g(rgb[1]), b(rgb[2]), a(a) {}
	FColor(ColorId);

	operator float4() const { return float4(v); }
	float3 rgb() const { return float3(r, g, b); }

	FColor operator*(float s) const { return FColor(r * s, g * s, b * s, a * s); }
	FColor operator*(const FColor &rhs) const {
		return FColor(r * rhs.r, g * rhs.g, b * rhs.b, a * rhs.a);
	}
	FColor operator-(const FColor &rhs) const {
		return FColor(r - rhs.r, g - rhs.g, b - rhs.b, a - rhs.a);
	}
	FColor operator+(const FColor &rhs) const {
		return FColor(r + rhs.r, g + rhs.g, b + rhs.b, a + rhs.a);
	}

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(FColor, r, g, b, a);

	union {
		struct {
			float r, g, b, a;
		};
		float v[4];
	};
};

float srgbToLinear(float);
float linearToSrgb(float);
float3 srgbToLinear(float3);
float3 linearToSrgb(float3);
FColor srgbToLinear(const FColor &);
FColor linearToSrgb(const FColor &);

void srgbToLinear(CSpan<IColor>, Span<FColor>);
void linearToSrgb(CSpan<FColor>, Span<IColor>);

FColor mulAlpha(FColor color, float alpha);
FColor desaturate(FColor col, float value);
float3 hsvToRgb(float3);
float3 rgbToHsv(float3);
inline float3 rgbToHsv(const FColor &col) { return rgbToHsv(col.rgb()); }
FColor gradientLerp(CSpan<FColor> colors, CSpan<float> values, float value);
FColor gradientLerp(CSpan<FColor> colors, float value);

// 32-bit RGBA color (8 bit per channel)
struct IColor {
	IColor(u8 r, u8 g, u8 b, u8 a = 255) : r(r), g(g), b(b), a(a) {}
	IColor(int4 rgba) : IColor(rgba[0], rgba[1], rgba[2], rgba[3]) {}
	IColor(int r, int g, int b, int a = 255)
		: r(clamp(r, 0, 255)), g(clamp(g, 0, 255)), b(clamp(b, 0, 255)), a(clamp(a, 0, 255)) {}
	explicit IColor(const FColor &c)
		: r(clamp(c.r * 255.0f, 0.0f, 255.0f)), g(clamp(c.g * 255.0f, 0.0f, 255.0f)),
		  b(clamp(c.b * 255.0f, 0.0f, 255.0f)), a(clamp(c.a * 255.0f, 0.0f, 255.0f)) {}
	IColor(IColor col, u8 alpha) : r(col.r), g(col.g), b(col.b), a(alpha) {}
	IColor(ColorId color_id) : IColor(FColor(color_id)) {}
	IColor() : IColor(0, 0, 0) {}

	operator FColor() const { return FColor(r, g, b, a) * (1.0f / 255.0f); }
	explicit operator int4() const { return {r, g, b, a}; }
	explicit operator float4() const { return float4(r, g, b, a) * (1.0f / 255.0f); }
	explicit operator int3() const { return {r, g, b}; }
	explicit operator float3() const { return float3(r, g, b) * (1.0f / 255.0f); }

	IColor bgra() const { return IColor(b, g, r, a); }

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(IColor, r, g, b, a);

	union {
		struct {
			u8 r, g, b, a;
		};
		u8 rgba[4];
	};
};

TextParser &operator>>(TextParser &, FColor &) EXCEPT;
TextParser &operator>>(TextParser &, IColor &) EXCEPT;
}
