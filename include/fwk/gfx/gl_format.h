// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"

namespace fwk {

// TODO: input/output format for pixel operations
// TODO: rename GlFormat to something else ?

// clang-format off

// Represents internal OpenGL formats;
// More info: https://www.khronos.org/opengl/wiki/Image_Format.html
// Suffixes:
// - (default, no suffix): unsigned normalized int, values from 0 to 1
// - s:  signed normalized int, values from -1 to 1
// - ui: unsigned int, values from 0 to max_int
// - i:  signed int, values from min_int to max_int
// - f:  floating point
DEFINE_ENUM(GlFormat,
	// 8-bit formats
	r8, r8s, r8ui, r8i,							 // 1 channel  8-bit

	// 16-bit formats
	r16, r16s, r16ui, r16i, r16f,				 // 1 channel 16-bit
	rg8, rg8s, rg8ui, rg8i,						 // 2 channel  8-bit

	// 24-bit formats
	srgb8, rgb8, rgb8s, rgb8ui, rgb8i,			 // 3 channel  8-bit

	// 32-bit formats
	srgba8, rgba8, rgba8s, rgba8ui, rgba8i,		 // 4 channel  8-bit
	rg16, rg16s, rg16ui, rg16i, rg16f,			 // 2 channel 16-bit
	r32ui, r32i, r32f,							 // 1 channel 32-bit

	// 48-bit formats
	rgb16, rgb16s, rgb16ui, rgb16i, rgb16f, 	 // 3 channel 16-bit

	// 64-bit formats
	rgba16, rgba16s, rgba16ui, rgba16i, rgba16f, // 4 channel 16-bit
	rg32ui, rg32i, rg32f,						 // 2 channel 32-bit

	// 96-bit formats
	rgb32ui, rgb32i, rgb32f,					 // 3 channel 32-bit

	// 128-bit formats
	rgba32ui, rgba32i, rgba32f, 				 // 4 channel 32-bit

	// Depth / stencil formats
	depth16, depth24, depth32, depth32f,
	depth24_stencil8, depth32f_stencil8,

	// BC1 (DXT1) formats (64-bit block size)
	srgb_bc1, rgb_bc1, srgba_bc1, rgba_bc1,

	// BC2 & BC3 (DXT3 & DXT5) formats (128-bit block size)
	srgba_bc2, rgba_bc2, srgba_bc3, rgba_bc3,

	// BC6H & BC7 formats (128-bit block size)
	srgba_bc7, rgba_bc7, rgb_bc6h, rgb_bc6h_signed
);

struct GlPackedFormat {
	int pixel_format;
	int pixel_type;
};

GlPackedFormat glPackedFormat(GlFormat);
int glInternalFormat(GlFormat);
int numComponents(GlFormat);
// Only makes sense for uncompressed formats
int bytesPerPixel(GlFormat);

bool hasDepthComponent(GlFormat);
bool hasStencilComponent(GlFormat);

bool isCompressed(GlFormat);
bool isFloatingPoint(GlFormat);

// All sizes in bytes; Returns 0 for depth/stencil formats
int compressedBlockSize(GlFormat);
int imageSize(GlFormat, int width, int height);
int imageRowSize(GlFormat, int width);
}
