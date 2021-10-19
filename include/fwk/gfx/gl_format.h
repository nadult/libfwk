// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"

namespace fwk {

DEFINE_ENUM(GlFormat, rgba, bgra, rgba_f16, rgba_f32, rgb, rgb_f16, rgb_f32, r32i, r32ui, r8, rg8,
			luminance, dxt1, dxt3, dxt5, depth16, depth24, depth32, depth32f, depth_stencil,
			stencil);

int glInternalFormat(GlFormat);
int glPixelFormat(GlFormat);
int glDataType(GlFormat);

// Returns 0 for compressed formats
int bytesPerPixel(GlFormat);

int evalImageSize(GlFormat, int width, int height);
int evalLineSize(GlFormat, int width);

bool hasDepthComponent(GlFormat);
bool hasStencilComponent(GlFormat);
bool isCompressed(GlFormat);
}
