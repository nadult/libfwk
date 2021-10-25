// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_format.h"

#include "fwk/algorithm.h"
#include "fwk/enum_map.h"
#include "fwk/gfx/opengl.h"

namespace fwk {

using Id = GlFormat;

namespace {
	struct FormatDesc {
		u8 bytes_per_pixel;
		u8 num_components;
		u16 internal_format;
		u16 packed_format = 0;
		u16 packed_type = 0;
	};

	// clang-format off
	const EnumMap<Id, FormatDesc> descs{{
		// 8-bit
		{Id::r8,     {1, 1, GL_R8,			GL_RED,			GL_UNSIGNED_BYTE}},
		{Id::r8s,    {1, 1, GL_R8_SNORM,	GL_RED,			GL_UNSIGNED_BYTE}},
		{Id::r8ui,   {1, 1, GL_R8UI,		GL_RED_INTEGER,	GL_UNSIGNED_BYTE}},
		{Id::r8i,    {1, 1, GL_R8I,			GL_RED_INTEGER,	GL_BYTE}},

		// 16-bit
		{Id::r16,    {2, 1, GL_R16,			GL_RED, 		GL_UNSIGNED_SHORT}},
		{Id::r16s,   {2, 1, GL_R16_SNORM,	GL_RED,			GL_UNSIGNED_SHORT}},
		{Id::r16ui,  {2, 1, GL_R16UI,		GL_RED_INTEGER,	GL_UNSIGNED_SHORT}},
		{Id::r16i,   {2, 1, GL_R16I,		GL_RED_INTEGER,	GL_SHORT}},
		{Id::r16f,   {2, 1, GL_R16F,		GL_RED,			GL_FLOAT}},

		{Id::rg8,    {2, 2, GL_RG8,			GL_RG,			GL_UNSIGNED_BYTE}},
		{Id::rg8s,   {2, 2, GL_RG8_SNORM,	GL_RG,			GL_UNSIGNED_BYTE}},
		{Id::rg8ui,  {2, 2, GL_RG8UI,		GL_RG_INTEGER,	GL_UNSIGNED_BYTE}},
		{Id::rg8i,   {2, 2, GL_RG8I,		GL_RG_INTEGER,	GL_BYTE}},

		// 24-bit
		{Id::srgb8,  {3, 3, GL_SRGB8,		GL_RGB,			GL_UNSIGNED_BYTE}},
		{Id::rgb8,   {3, 3, GL_RGB8,		GL_RGB,			GL_UNSIGNED_BYTE}},
		{Id::rgb8s,  {3, 3, GL_RGB8_SNORM,	GL_RGB,			GL_UNSIGNED_BYTE}},
		{Id::rgb8ui, {3, 3, GL_RGB8UI,		GL_RGB_INTEGER,	GL_UNSIGNED_BYTE}},
		{Id::rgb8i,  {3, 3, GL_RGB8I,		GL_RGB_INTEGER,	GL_BYTE}},

		// 32-bit
		{Id::srgba8,  {4, 4, GL_SRGB8_ALPHA8,	GL_RGBA,			GL_UNSIGNED_BYTE}},
		{Id::rgba8,   {4, 4, GL_RGBA8,			GL_RGBA,			GL_UNSIGNED_BYTE}},
		{Id::rgba8s,  {4, 4, GL_RGBA8_SNORM,	GL_RGBA,			GL_UNSIGNED_BYTE}},
		{Id::rgba8ui, {4, 4, GL_RGBA8UI,		GL_RGBA_INTEGER,	GL_UNSIGNED_BYTE}},
		{Id::rgba8i,  {4, 4, GL_RGBA8I,			GL_RGBA_INTEGER,	GL_UNSIGNED_BYTE}},

		{Id::rg16,    {4, 2, GL_RG16,			GL_RG,				GL_UNSIGNED_SHORT}},
		{Id::rg16s,   {4, 2, GL_RG16_SNORM,		GL_RG,				GL_UNSIGNED_SHORT}},
		{Id::rg16ui,  {4, 2, GL_RG16UI,			GL_RG_INTEGER,		GL_UNSIGNED_SHORT}},
		{Id::rg16i,   {4, 2, GL_RG16I,			GL_RG_INTEGER,		GL_SHORT}},
		{Id::rg16f,   {4, 2, GL_RG16F,			GL_RG,				GL_HALF_FLOAT}},

		{Id::r32ui,   {4, 1, GL_R32UI,			GL_RED_INTEGER,		GL_UNSIGNED_INT}},
		{Id::r32i,    {4, 1, GL_R32I,			GL_RED_INTEGER,		GL_INT}},
		{Id::r32f,    {4, 1, GL_R32F,			GL_RED,				GL_FLOAT}},
		
		// 48-bit
		{Id::rgb16,    {6, 3, GL_RGB16,			GL_RGB,				GL_UNSIGNED_SHORT}},
		{Id::rgb16s,   {6, 3, GL_RGB16_SNORM,	GL_RGB,				GL_UNSIGNED_SHORT}},
		{Id::rgb16ui,  {6, 3, GL_RGB16UI,		GL_RGB_INTEGER,		GL_UNSIGNED_SHORT}},
		{Id::rgb16i,   {6, 3, GL_RGB16I,		GL_RGB_INTEGER,		GL_SHORT}},
		{Id::rgb16f,   {6, 3, GL_RGB16F,		GL_RGB,				GL_HALF_FLOAT}},

		// 64-bit
		{Id::rgba16,   {8, 4, GL_RGBA16,		GL_RGBA,			GL_UNSIGNED_SHORT}},
		{Id::rgba16s,  {8, 4, GL_RGBA16_SNORM,	GL_RGBA,			GL_UNSIGNED_SHORT}},
		{Id::rgba16ui, {8, 4, GL_RGBA16UI,		GL_RGBA_INTEGER,	GL_UNSIGNED_SHORT}},
		{Id::rgba16i,  {8, 4, GL_RGBA16I,		GL_RGBA_INTEGER,	GL_SHORT}},
		{Id::rgba16f,  {8, 4, GL_RGBA16F,		GL_RGBA,			GL_HALF_FLOAT}},

		{Id::rg32ui,   {8, 2, GL_RG32UI,		GL_RG_INTEGER,		GL_UNSIGNED_INT}},
		{Id::rg32i,    {8, 2, GL_RG32I,			GL_RG_INTEGER,		GL_INT}},
		{Id::rg32f,    {8, 2, GL_RG32F,			GL_RG,				GL_FLOAT}},

		// 96-bit
		{Id::rgb32ui,  {12, 3, GL_RGB32UI,		GL_RGB_INTEGER,		GL_UNSIGNED_INT}},
		{Id::rgb32i,   {12, 3, GL_RGB32I,		GL_RGB_INTEGER,		GL_INT}},
		{Id::rgb32f,   {12, 3, GL_RGB32F,		GL_RGB,				GL_FLOAT}},

		// 128-bit
		{Id::rgba32ui, {16, 4, GL_RGBA32UI,		GL_RGBA_INTEGER,	GL_UNSIGNED_INT}},
		{Id::rgba32i,  {16, 4, GL_RGBA32I,		GL_RGBA_INTEGER,	GL_INT}},
		{Id::rgba32f,  {16, 4, GL_RGBA32F,		GL_RGBA,			GL_FLOAT}},

		// Depth/stencil
		{Id::depth16,           {0, 1, GL_DEPTH_COMPONENT16,	GL_DEPTH_COMPONENT,	GL_UNSIGNED_SHORT}},
		{Id::depth24,           {0, 1, GL_DEPTH_COMPONENT24}},
		{Id::depth32,           {0, 1, GL_DEPTH_COMPONENT32,	GL_DEPTH_COMPONENT,	GL_UNSIGNED_INT}},
		{Id::depth32f,          {0, 1, GL_DEPTH_COMPONENT32F,	GL_DEPTH_COMPONENT,	GL_FLOAT}},
		{Id::depth24_stencil8,  {0, 2, GL_DEPTH24_STENCIL8,		GL_DEPTH_STENCIL,	GL_UNSIGNED_INT}},
		{Id::depth32f_stencil8, {0, 2, GL_DEPTH32F_STENCIL8}},

		// BC1
		{Id::srgb_bc1,  {0, 3, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT}},
		{Id::rgb_bc1,   {0, 3, GL_COMPRESSED_RGB_S3TC_DXT1_EXT}},
		{Id::srgba_bc1, {0, 4, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT}},
		{Id::rgba_bc1,  {0, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT}},

		// BC2 & BC3
		{Id::srgba_bc2,  {0, 4, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT}},
		{Id::rgba_bc2,   {0, 4, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT}},
		{Id::srgba_bc3,  {0, 4, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT}},
		{Id::rgba_bc3,   {0, 4, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT}},

		// BPTC (BC6 - BC7)
		{Id::rgba_bc7,          {0, 4, GL_COMPRESSED_RGBA_BPTC_UNORM}},
		{Id::srgba_bc7,         {0, 4, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM}},
		{Id::rgb_bc6h,          {0, 3, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT}},
		{Id::rgb_bc6h_signed,   {0, 3, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT}},
	}};
	// clang-format on
}

// TODO: better naming of these functions?

GlPackedFormat glPackedFormat(GlFormat format) {
	auto &desc = descs[format];
	return {desc.packed_format, desc.packed_type};
}

int glInternalFormat(GlFormat format) { return descs[format].internal_format; }
int bytesPerPixel(GlFormat format) { return descs[format].bytes_per_pixel; }
int numComponents(GlFormat format) { return descs[format].num_components; }
bool hasDepthComponent(GlFormat format) {
	return format >= Id::depth16 && format <= GlFormat::depth32f_stencil8;
}
bool hasStencilComponent(GlFormat format) {
	return format >= Id::depth24_stencil8 && format <= Id::depth32f_stencil8;
}

bool isCompressed(GlFormat format) { return format >= Id::srgb_bc1; }
bool isFloatingPoint(GlFormat format) {
	return isOneOf(format, Id::r16f, Id::rg16f, Id::r32f, Id::rgb16f, Id::rgba16f, Id::rg32f,
				   Id::rgb32f, Id::rgba32f);
}

int compressedBlockSize(GlFormat format) {
	return format < Id::srgb_bc1 ? 0 : format < Id::srgba_bc2 ? 8 : 16;
}
int imageSize(GlFormat format, int width, int height) {
	if(isCompressed(format))
		return ((width + 3) / 4) * ((height + 3) / 4) * compressedBlockSize(format);
	return width * height * bytesPerPixel(format);
}
int imageRowSize(GlFormat format, int width) {
	return ((width + 3) / 4) * compressedBlockSize(format);
}
}
