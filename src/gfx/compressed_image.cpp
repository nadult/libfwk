// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/compressed_image.h"

#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/image.h"

#define STB_DXT_STATIC
#define STB_DXT_IMPLEMENTATION

#include "../extern/stb_dxt.h"

namespace fwk {

using Format = VBlockFormat;

int compressedBlockSize(VBlockFormat format) {
	if(isOneOf(format, Format::r_bc4, Format::r_bc4s))
		return 8;
	return format < VBlockFormat::srgba_bc2 ? 8 : 16;
}

int imageSize(VBlockFormat format, int width, int height) {
	return ((width + 3) / 4) * ((height + 3) / 4) * compressedBlockSize(format);
}

int imageRowSize(VBlockFormat format, int width) {
	return ((width + 3) / 4) * compressedBlockSize(format);
}

CompressedImage::CompressedImage(PodVector<u8> data, int2 size, Format format)
	: m_data(move(data)), m_size(size), m_format(format) {
	DASSERT(m_data.size() >= imageSize(format, size.x, size.y));
}

CompressedImage::CompressedImage(const Image &input, Format format)
	: m_size(input.size()), m_format(format) {
	DASSERT(
		isOneOf(format, Format::rgb_bc1, Format::srgb_bc1, Format::rgba_bc3, Format::srgba_bc3));

	m_data.resize(imageSize(format, m_size.x, m_size.y));
	IColor input_data[4 * 4];
	int bh = (m_size.y + 3) / 4, bw = (m_size.x + 3) / 4;
	u8 *dst = m_data.data();

	int block_size = compressedBlockSize(format);
	bool is_dxt5 = block_size == 16;

	for(int by = 0; by < bh; by++) {
		int sy = min(m_size.y - by * 4, 4);
		fill(input_data, ColorId::black);
		for(int bx = 0; bx < bw; bx++) {
			int sx = min(m_size.x - bx * 4, 4);
			const IColor *block_src = &input(bx * 4, by * 4);
			for(int y = 0; y < sy; y++)
				copy(input_data + y * 4, cspan(block_src + m_size.x * y, sx));

			stb_compress_dxt_block(dst, reinterpret_cast<const u8 *>(input_data), is_dxt5,
								   STB_DXT_HIGHQUAL);
			dst += block_size;
		}
	}
}

CompressedImage::~CompressedImage() = default;
}
