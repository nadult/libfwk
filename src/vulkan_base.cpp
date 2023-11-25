// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan_base.h"

#include "fwk/enum_map.h"
#include "fwk/vulkan/vulkan_internal.h"

namespace fwk {

VSamplerSetup::VSamplerSetup(VTexFilter mag_filter, VTexFilter min_filter,
							 Maybe<VTexFilter> mip_filter, VTexAddress address_mode,
							 uint max_anisotropy_samples)
	: mag_filter(mag_filter), min_filter(min_filter), mipmap_filter(mip_filter),
	  address_mode{address_mode, address_mode, address_mode},
	  max_anisotropy_samples(max_anisotropy_samples) {}

VSamplerSetup::VSamplerSetup(VTexFilter mag_filter, VTexFilter min_filter,
							 Maybe<VTexFilter> mip_filter, array<VTexAddress, 3> address_mode,
							 uint max_anisotropy_samples)
	: mag_filter(mag_filter), min_filter(min_filter), mipmap_filter(mip_filter),
	  address_mode(address_mode), max_anisotropy_samples(max_anisotropy_samples) {}

int primitiveCount(VPrimitiveTopology topo, int vertex_count) {
	using Topo = VPrimitiveTopology;
	switch(topo) {
	case Topo::point_list:
		return vertex_count;
	case Topo::line_list:
		return vertex_count / 2;
	case Topo::line_strip:
		return max(0, vertex_count - 1);
	case Topo::triangle_list:
		return vertex_count / 3;
	case Topo::triangle_strip:
	case Topo::triangle_fan:
		return max(0, vertex_count - 2);
	}
	return 0;
}

struct VFormatInfo {
	VFormatInfo(VBaseFormat base, VNumericFormat numeric, VkFormat vk_format)
		: base(base), numeric(numeric), vk_format(vk_format) {}
	VFormatInfo() {}

	VBaseFormat base = VBaseFormat(0);
	VNumericFormat numeric = VNumericFormat(0);
	u8 vk_format = 0;
};

#define DEF(format, base, numeric, vk)                                                             \
	{                                                                                              \
		VFormat::format, { VBaseFormat::base, VNumericFormat::numeric, vk }                        \
	}

// clang-format off
static const EnumMap<VBaseFormat, u8> base_unit_size{{
	1, 2, 3,  3,  4, 4,	 4,     //  8-bit components
	4, 4,                       //  2-bit alpha
	2, 4, 6,  8,                // 16-bit components
	4, 8, 12, 16,               // 32-bit components
	4, 4,                       // special
	8, 8, 16, 16, 8, 16, 16, 16 // block
}};

static const EnumMap<VFormat, VFormatInfo> format_infos{{
	//  8-bit R8
	DEF(r8_unorm, r8, unorm, VK_FORMAT_R8_UNORM),
	DEF(r8_snorm, r8, snorm, VK_FORMAT_R8_SNORM),
	DEF(r8_uint,  r8, uint,  VK_FORMAT_R8_UINT),
	DEF(r8_sint,  r8, sint,  VK_FORMAT_R8_SINT),
	DEF(r8_srgb,  r8, srgb,  VK_FORMAT_R8_SRGB),

	// 16-bit (R8, G8)
	DEF(rg8_unorm, rg8, unorm, VK_FORMAT_R8G8_UNORM),
	DEF(rg8_snorm, rg8, snorm, VK_FORMAT_R8G8_SNORM),
	DEF(rg8_uint,  rg8, uint,  VK_FORMAT_R8G8_UINT),
	DEF(rg8_sint,  rg8, sint,  VK_FORMAT_R8G8_SINT),
	DEF(rg8_srgb,  rg8, srgb,  VK_FORMAT_R8G8_SRGB),

	// 24-bit (R8, G8, B8)
	DEF(rgb8_unorm, rgb8, unorm, VK_FORMAT_R8G8B8_UNORM),
	DEF(rgb8_snorm, rgb8, snorm, VK_FORMAT_R8G8B8_SNORM),
	DEF(rgb8_uint,  rgb8, uint,  VK_FORMAT_R8G8B8_UINT),
	DEF(rgb8_sint,  rgb8, sint,  VK_FORMAT_R8G8B8_SINT),
	DEF(rgb8_srgb,  rgb8, srgb,  VK_FORMAT_R8G8B8_SRGB),

	// 24-bit (B8, G8, R8)
	DEF(bgr8_unorm, bgr8, unorm, VK_FORMAT_B8G8R8_UNORM),
	DEF(bgr8_snorm, bgr8, snorm, VK_FORMAT_B8G8R8_SNORM),
	DEF(bgr8_uint,  bgr8, uint,  VK_FORMAT_B8G8R8_UINT),
	DEF(bgr8_sint,  bgr8, sint,  VK_FORMAT_B8G8R8_SINT),
	DEF(bgr8_srgb,  bgr8, srgb,  VK_FORMAT_B8G8R8_SRGB),

	// 32-bit (R8, G8, B8, A8)
	DEF(rgba8_unorm, rgba8, unorm, VK_FORMAT_R8G8B8A8_UNORM),
	DEF(rgba8_snorm, rgba8, snorm, VK_FORMAT_R8G8B8A8_SNORM),
	DEF(rgba8_uint,  rgba8, uint,  VK_FORMAT_R8G8B8A8_UINT),
	DEF(rgba8_sint,  rgba8, sint,  VK_FORMAT_R8G8B8A8_SINT),
	DEF(rgba8_srgb,  rgba8, srgb,  VK_FORMAT_R8G8B8A8_SRGB),

	// 32-bit (B8, G8, R8, A8)
	DEF(bgra8_unorm, bgra8, unorm, VK_FORMAT_B8G8R8A8_UNORM),
	DEF(bgra8_snorm, bgra8, snorm, VK_FORMAT_B8G8R8A8_SNORM),
	DEF(bgra8_uint,  bgra8, uint,  VK_FORMAT_B8G8R8A8_UINT),
	DEF(bgra8_sint,  bgra8, sint,  VK_FORMAT_B8G8R8A8_SINT),
	DEF(bgra8_srgb,  bgra8, srgb,  VK_FORMAT_B8G8R8A8_SRGB),

	// 32-bit (A8, B8, G8, R8), packed
	DEF(abgr8_unorm, abgr8, unorm, VK_FORMAT_A8B8G8R8_UNORM_PACK32),
	DEF(abgr8_snorm, abgr8, snorm, VK_FORMAT_A8B8G8R8_SNORM_PACK32),
	DEF(abgr8_uint,  abgr8, uint,  VK_FORMAT_A8B8G8R8_UINT_PACK32),
	DEF(abgr8_sint,  abgr8, sint,  VK_FORMAT_A8B8G8R8_SINT_PACK32),
	DEF(abgr8_srgb,  abgr8, srgb,  VK_FORMAT_A8B8G8R8_SRGB_PACK32),

	// 32-bit (A2, R10, G10, B10), packed
	DEF(a2rgb10_unorm, a2rgb10, unorm, VK_FORMAT_A2R10G10B10_UNORM_PACK32),
	DEF(a2rgb10_snorm, a2rgb10, snorm, VK_FORMAT_A2R10G10B10_SNORM_PACK32),
	DEF(a2rgb10_uint,  a2rgb10, uint,  VK_FORMAT_A2R10G10B10_UINT_PACK32),
	DEF(a2rgb10_sint,  a2rgb10, sint,  VK_FORMAT_A2R10G10B10_SINT_PACK32),
	// 32-bit (A2, B10, G10, R10), packed
	DEF(a2bgr10_unorm, a2bgr10, unorm, VK_FORMAT_A2B10G10R10_UNORM_PACK32),
	DEF(a2bgr10_snorm, a2bgr10, snorm, VK_FORMAT_A2B10G10R10_SNORM_PACK32),
	DEF(a2bgr10_uint,  a2bgr10, uint,  VK_FORMAT_A2B10G10R10_UINT_PACK32),
	DEF(a2bgr10_sint,  a2bgr10, sint,  VK_FORMAT_A2B10G10R10_SINT_PACK32),

	//  16-bit R16
	DEF(r16_unorm,     r16,    unorm,  VK_FORMAT_R16_UNORM),
	DEF(r16_snorm,     r16,    snorm,  VK_FORMAT_R16_SNORM),
	DEF(r16_uint,      r16,    uint,   VK_FORMAT_R16_UINT),
	DEF(r16_sint,      r16,    sint,   VK_FORMAT_R16_SINT),
	DEF(r16_sfloat,    r16,    sfloat, VK_FORMAT_R16_SFLOAT),
	//  32-bit (R16, G16)
	DEF(rg16_unorm,    rg16,   unorm,  VK_FORMAT_R16G16_UNORM),
	DEF(rg16_snorm,    rg16,   snorm,  VK_FORMAT_R16G16_SNORM),
	DEF(rg16_uint,     rg16,   uint,   VK_FORMAT_R16G16_UINT),
	DEF(rg16_sint,     rg16,   sint,   VK_FORMAT_R16G16_SINT),
	DEF(rg16_sfloat,   rg16,   sfloat, VK_FORMAT_R16G16_SFLOAT),
	//  48-bit (R16, G16, B16)
	DEF(rgb16_unorm,   rgb16,  unorm,  VK_FORMAT_R16G16B16_UNORM),
	DEF(rgb16_snorm,   rgb16,  snorm,  VK_FORMAT_R16G16B16_SNORM),
	DEF(rgb16_uint,    rgb16,  uint,   VK_FORMAT_R16G16B16_UINT),
	DEF(rgb16_sint,    rgb16,  sint,   VK_FORMAT_R16G16B16_SINT),
	DEF(rgb16_sfloat,  rgb16,  sfloat, VK_FORMAT_R16G16B16_SFLOAT),
	//  64-bit (R16, G16, B16, A16)
	DEF(rgba16_unorm,  rgba16, unorm,  VK_FORMAT_R16G16B16A16_UNORM),
	DEF(rgba16_snorm,  rgba16, snorm,  VK_FORMAT_R16G16B16A16_SNORM),
	DEF(rgba16_uint,   rgba16, uint,   VK_FORMAT_R16G16B16A16_UINT),
	DEF(rgba16_sint,   rgba16, sint,   VK_FORMAT_R16G16B16A16_SINT),
	DEF(rgba16_sfloat, rgba16, sfloat, VK_FORMAT_R16G16B16A16_SFLOAT),

	//   32-bit R32
	DEF(r32_uint,      r32,    uint,   VK_FORMAT_R32_UINT),
	DEF(r32_sint,      r32,    sint,   VK_FORMAT_R32_SINT),
	DEF(r32_sfloat,    r32,    sfloat, VK_FORMAT_R32_SFLOAT),
	//   64-bit (R32, G32)
	DEF(rg32_uint,     rg32,   uint,   VK_FORMAT_R32G32_UINT),
	DEF(rg32_sint,     rg32,   sint,   VK_FORMAT_R32G32_SINT),
	DEF(rg32_sfloat,   rg32,   sfloat, VK_FORMAT_R32G32_SFLOAT),
	//   96-bit (R32, G32, B32)
	DEF(rgb32_uint,    rgb32,  uint,   VK_FORMAT_R32G32B32_UINT),
	DEF(rgb32_sint,    rgb32,  sint,   VK_FORMAT_R32G32B32_SINT),
	DEF(rgb32_sfloat,  rgb32,  sfloat, VK_FORMAT_R32G32B32_SFLOAT),
	//  128-bit (R32, G32, B32, A32)
	DEF(rgba32_uint,   rgba32, uint,   VK_FORMAT_R32G32B32A32_UINT),
	DEF(rgba32_sint,   rgba32, sint,   VK_FORMAT_R32G32B32A32_SINT),
	DEF(rgba32_sfloat, rgba32, sfloat, VK_FORMAT_R32G32B32A32_SFLOAT),

	// 32-bit packed formats
	DEF(b10g11r11_ufloat, b10g11r11_ufloat, ufloat, VK_FORMAT_B10G11R11_UFLOAT_PACK32),
	DEF(e5r9g9b9_ufloat,  e5r9g9b9_ufloat,  ufloat, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32),

	//  64-bit 4x4 BC1 (DXT1)
	DEF(bc1_rgb_unorm,   bc1_rgb,  unorm, VK_FORMAT_BC1_RGB_UNORM_BLOCK),
	DEF(bc1_rgb_srgb,    bc1_rgb,  srgb,  VK_FORMAT_BC1_RGB_SRGB_BLOCK),
	DEF(bc1_rgba_unorm,  bc1_rgba, unorm, VK_FORMAT_BC1_RGBA_UNORM_BLOCK),
	DEF(bc1_rgba_srgb,   bc1_rgba, srgb,  VK_FORMAT_BC1_RGBA_SRGB_BLOCK),
	// 128-bit 4x4 BC2 & BC3 (DXT3 & DXT5)
	DEF(bc2_rgba_unorm,  bc2_rgba, unorm, VK_FORMAT_BC2_UNORM_BLOCK),
	DEF(bc2_rgba_srgb,   bc2_rgba, srgb,  VK_FORMAT_BC2_SRGB_BLOCK),
	DEF(bc3_rgba_unorm,  bc3_rgba, unorm, VK_FORMAT_BC3_UNORM_BLOCK),
	DEF(bc3_rgba_srgb,   bc3_rgba, srgb,  VK_FORMAT_BC3_SRGB_BLOCK),
	//  64-bit 4x4 BC4 (single channel)
	DEF(bc4_r_unorm,     bc4_r,    unorm, VK_FORMAT_BC4_UNORM_BLOCK),
	DEF(bc4_r_snorm,     bc4_r,    snorm, VK_FORMAT_BC4_SNORM_BLOCK),
	// 128-bit 4x4 BC5 (two channels)
	DEF(bc5_rg_unorm,    bc5_rg,   unorm, VK_FORMAT_BC5_UNORM_BLOCK),
	DEF(bc5_rg_snorm,    bc5_rg,   snorm, VK_FORMAT_BC5_SNORM_BLOCK),
	// 128-bit 4x4 BC6H
	DEF(bc6h_rgb_ufloat, bc1_rgb,  ufloat, VK_FORMAT_BC6H_UFLOAT_BLOCK),
	DEF(bc6h_rgb_sfloat, bc1_rgb,  sfloat, VK_FORMAT_BC6H_SFLOAT_BLOCK),
	// 128-bit 4x4 BC7
	DEF(bc7_rgba_unorm,  bc7_rgba,  unorm, VK_FORMAT_BC7_UNORM_BLOCK),
	DEF(bc7_rgba_srgb,   bc7_rgba,  srgb,  VK_FORMAT_BC7_SRGB_BLOCK),
}};

// clang-format on
#undef DEF

VBaseFormat baseFormat(VFormat format) { return format_infos[format].base; }
VNumericFormat numericFormat(VFormat format) { return format_infos[format].numeric; }

Maybe<VFormat> makeFormat(VBaseFormat base, VNumericFormat numeric) {
	auto it =
		std::lower_bound(format_infos.begin(), format_infos.end(), base,
						 [](const VFormatInfo &lhs, VBaseFormat base) { return lhs.base < base; });
	while(it != format_infos.end() && it->base == base) {
		if(it->numeric == numeric)
			return VFormat(it - format_infos.begin());
		++it;
	}
	return none;
}

int unitByteSize(VFormat format) { return unitByteSize(baseFormat(format)); }
int unitByteSize(VBaseFormat format) { return base_unit_size[format]; }

int2 imageBlockSize(VFormat format, int2 pixel_size) {
	int unit_size = unitSize(format);
	return unit_size == 1 ? pixel_size : (pixel_size + int2(unit_size - 1)) / unit_size;
}

int imageByteSize(VFormat format, int2 pixel_size) {
	auto block_size = imageBlockSize(format, pixel_size);
	return block_size.x * block_size.y * unitByteSize(format);
}

bool areCompatible(VFormat lhs, VFormat rhs) {
	return unitSize(lhs) == unitSize(rhs) && unitByteSize(lhs) == unitByteSize(rhs);
}

VkFormat toVk(VFormat format) { return VkFormat(format_infos[format].vk_format); }
Maybe<VFormat> fromVk(VkFormat format) {
	auto it = std::lower_bound(
		format_infos.begin(), format_infos.end(), format,
		[](const VFormatInfo &lhs, VkFormat fmt) { return VkFormat(lhs.vk_format) < fmt; });
	if(it == format_infos.end() || VkFormat(it->vk_format) != format)
		return none;
	return VFormat(it - format_infos.begin());
}
}