// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/float_image.h"
#include "fwk/gfx/image.h"

#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include "../extern/stb_image_resize.h"

namespace fwk {

Image Image::rescale(int2 new_size, ImageRescaleOpts opts) const {
	Image new_image(new_size, no_init);

	auto input_pixels = reinterpret_cast<const u8 *>(m_data.data());
	auto output_pixels = reinterpret_cast<u8 *>(new_image.m_data.data());

	int alpha_opt = 3;
	int flags = opts & ImageRescaleOpt::premultiplied_alpha ? STBIR_FLAG_ALPHA_PREMULTIPLIED : 0;
	auto colorspace =
		opts & ImageRescaleOpt::srgb ? STBIR_COLORSPACE_SRGB : STBIR_COLORSPACE_LINEAR;
	int result = stbir_resize_uint8_generic(
		input_pixels, width(), height(), 0, output_pixels, new_size.x, new_size.y, 0, 4, alpha_opt,
		flags, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, colorspace, nullptr);
	if(!result)
		FATAL("STB_image_resize failed!");

	return new_image;
}

FloatImage FloatImage::rescale(int2 new_size, ImageRescaleOpts opts) const {
	FloatImage new_image(new_size, no_init);

	auto input_pixels = reinterpret_cast<const float *>(m_data.data());
	auto output_pixels = reinterpret_cast<float *>(new_image.m_data.data());

	int alpha_opt = 3;
	int flags = opts & ImageRescaleOpt::premultiplied_alpha ? STBIR_FLAG_ALPHA_PREMULTIPLIED : 0;
	auto colorspace =
		opts & ImageRescaleOpt::srgb ? STBIR_COLORSPACE_SRGB : STBIR_COLORSPACE_LINEAR;
	int result = stbir_resize_float_generic(
		input_pixels, width(), height(), 0, output_pixels, new_size.x, new_size.y, 0, 4, alpha_opt,
		flags, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, colorspace, nullptr);
	if(!result)
		FATAL("STB_image_resize failed!");

	return new_image;
}
}
