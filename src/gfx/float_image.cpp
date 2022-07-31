// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/float_image.h"

#include "fwk/str.h"

namespace fwk {

// TODO: shouldn't we use float4 internally instead of FColor ?
// Maybe we really should make a templated version of Image?

FloatImage::FloatImage() = default;
FloatImage::FloatImage(int2 size, NoInitTag) : m_data(size.x * size.y), m_size(size) {
	DASSERT(size.x >= 0 && size.y >= 0);
}

FloatImage::FloatImage(int2 size, FColor fill_color) : FloatImage(size, no_init) {
	fill(fill_color);
}

FloatImage::FloatImage(const Image &rhs, bool srgb_to_linear)
	: m_data(rhs.pixelCount()), m_size(rhs.size()) {
	for(int y = 0; y < m_size.y; y++) {
		auto src = rhs.row(y);
		auto dst = row(y);
		if(srgb_to_linear) {
			srgbToLinear(src, dst);
		} else {
			for(int x = 0; x < m_size.x; x++)
				dst[x] = FColor(src[x]);
		}
	}
}

FloatImage::FloatImage(PodVector<FColor> data, int2 size) : m_data(move(data)), m_size(size) {
	DASSERT(size.x >= 0 && size.y >= 0);
	DASSERT(m_data.size() >= size.x * size.y);
}
FloatImage::~FloatImage() = default;

void FloatImage::resize(int2 size, Maybe<FColor> fill_color) {
	if(size == m_size)
		return;
	FloatImage new_tex(size);
	if(fill_color)
		new_tex.fill(*fill_color);
	new_tex.blit(*this, int2(0, 0));
	swap(new_tex);
}

void FloatImage::premultiplyAlpha() {
	for(auto &pixel : m_data) {
		pixel.r *= pixel.a;
		pixel.g *= pixel.a;
		pixel.b *= pixel.a;
	}
}

void FloatImage::swap(FloatImage &tex) {
	std::swap(m_size, tex.m_size);
	m_data.swap(tex.m_data);
}

void FloatImage::clear() {
	m_size = int2();
	m_data.clear();
}

void FloatImage::fill(FColor color) {
	for(auto &col : m_data)
		col = color;
}

void FloatImage::blit(FloatImage const &src, int2 dst_pos) {
	int2 src_pos(0, 0), blit_size = src.size();
	if(dst_pos.x < 0) {
		src_pos.x = -dst_pos.x;
		blit_size.x += dst_pos.x;
		dst_pos.x = 0;
	}
	if(dst_pos.y < 0) {
		src_pos.y = -dst_pos.y;
		blit_size.y += dst_pos.y;
		dst_pos.y = 0;
	}

	blit_size = vmin(blit_size, size() - dst_pos);
	if(blit_size.x <= 0 || blit_size.y <= 0)
		return;

	// TODO: verify
	for(int y = 0; y < blit_size.y; y++) {
		auto src_row = src.row(src_pos.y + y).subSpan(src_pos.x);
		auto dst_row = row(dst_pos.y + y).subSpan(dst_pos.x, dst_pos.x + blit_size.x);
		copy(dst_row, src_row);
	}
}
}
