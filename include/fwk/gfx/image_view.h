// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/vulkan_base.h"

#include <iterator>

namespace fwk {

template <class T, int width, int height> struct PixelBlock {
	T pixels[width * height];
};

struct BCColorBlock {
	u16 color1, color2;
	u8 bits[4];
};

struct BCAlphaBlock {
	u8 alpha1, alpha2;
	u8 selectors[6];
};

using BC1Block = BCColorBlock;

struct BC2Block {
	u8 alpha[8];
	BCColorBlock color;
};

struct BC3Block {
	BCAlphaBlock alpha;
	BCColorBlock color;
};

using BC4Block = BCAlphaBlock;

struct BC5Block {
	BCAlphaBlock channel1;
	BCAlphaBlock channel2;
};

template <class T> struct PixelInfo {};

#define PIXEL_TYPE(Type, default_format_)                                                          \
	template <> struct PixelInfo<Type> {                                                           \
		static constexpr VFormat default_format = VFormat::default_format_;                        \
	};

PIXEL_TYPE(IColor, rgba8_unorm)
PIXEL_TYPE(FColor, rgba32_sfloat)
PIXEL_TYPE(u8, r8_unorm)
PIXEL_TYPE(u16, r16_unorm)
PIXEL_TYPE(u32, r32_uint)
PIXEL_TYPE(i8, r8_snorm)
PIXEL_TYPE(i16, r16_snorm)
PIXEL_TYPE(i32, r32_sint)
PIXEL_TYPE(int2, rg32_sint)
PIXEL_TYPE(int3, rgb32_sint)
PIXEL_TYPE(int4, rgba32_sint)
PIXEL_TYPE(short2, rg16_snorm)
PIXEL_TYPE(short3, rgb16_snorm)
PIXEL_TYPE(short4, rgba16_snorm)
PIXEL_TYPE(float, r32_sfloat)
PIXEL_TYPE(float2, rg32_sfloat)
PIXEL_TYPE(float3, rgb32_sfloat)
PIXEL_TYPE(float4, rgba32_sfloat)
PIXEL_TYPE(BC1Block, bc1_rgb_unorm)
PIXEL_TYPE(BC2Block, bc2_rgba_unorm)
PIXEL_TYPE(BC3Block, bc3_rgba_unorm)
PIXEL_TYPE(BC4Block, bc4_r_unorm)
PIXEL_TYPE(BC5Block, bc5_rg_unorm)

/*	DEFINE_ENUM(VBaseFormat, r8, rg8, rgb8, bgr8, rgba8, bgra8, abgr8, a2rgb10, a2bgr10, r16, rg16,
			rgb16, rgba16, r32, rg32, rgb32, rgba32, b10g11r11_ufloat, e5r9g9b9_ufloat, bc1_rgb,
			bc1_rgba, bc2_rgba, bc3_rgba, bc4_r, bc5_rg, bc6h_rgb, bc7_rgba);*/
// DEFINE_ENUM(VNumericFormat, unorm, snorm, uint, sint, ufloat, sfloat, srgb);

namespace detail {
	template <class T> struct IsPixelType {
		FWK_SFINAE_TEST(value, T, PixelInfo<U>::default_format);
	};
}

// TODO: move to gfx_base?
template <class T> constexpr bool is_pixel = detail::IsPixelType<T>::value;
template <class T> concept c_pixel = is_pixel<T>;

template <class T>
	requires c_pixel<RemoveConst<T>>
struct PixelInfo<const T> : public PixelInfo<T> {};

template <c_pixel T> class ImageIter {
  public:
	using iterator_category = std::forward_iterator_tag;
	using difference_type = void;
	using value_type = T;
	using pointer = T*;
	using reference = T&;

	ImageIter(T *data, int width, int stride)
		: m_current(data), m_row_end(data + width), m_width(width), m_stride(stride) {}

	T &operator*() const { return *m_current; }
	bool operator==(const ImageIter &rhs) const { return m_current == rhs.m_current; }
	bool operator<(const ImageIter &rhs) const { return m_current < rhs.m_current; }

	const ImageIter &operator++() {
		m_current++;
		if(m_current == m_row_end) {
			m_current += m_stride - m_width;
			m_row_end += m_stride;
		}
		return *this;
	}

  private:
	T *m_current, *m_row_end;
	int m_width, m_stride;
};

template <c_pixel T> class ImageView {
  public:
	ImageView(const ImageView<RemoveConst<T>> &rhs);
	ImageView(T *pixels, int2 size, int stride, VFormat format);
	ImageView();
	~ImageView() = default;

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }
	int pixelCount() const { return m_size.x * m_size.y; }

	bool inRange(int x, int y) const;

	VFormat format() const { return m_format; }
	bool empty() const { return m_size.x == 0 || m_size.y == 0; }

	Span<T> row(int y);
	CSpan<T> row(int y) const;

	// Iterating over rows can be a bit more efficient
	ImageIter<const T> begin() const;
	ImageIter<T> begin();
	ImageIter<const T> end() const;
	ImageIter<T> end();

	T &operator()(int2 pos);
	const T &operator()(int2 pos) const;

	T &operator()(int x, int y);
	const T &operator()(int x, int y) const;

	T &operator[](int idx) { return m_pixels[idx]; }
	const T &operator[](int idx) const { return m_pixels[idx]; }

  private:
	T *m_pixels;
	int2 m_size;
	int m_stride;
	VFormat m_format;
};

// -------------------------------------------------------------------------------------------
// ---  Inlined code -------------------------------------------------------------------------

template <c_pixel T>
ImageView<T>::ImageView(const ImageView<RemoveConst<T>> &rhs)
	: m_pixels(rhs.m_pixels), m_size(rhs.m_size), m_stride(rhs.m_stride), m_format(rhs.m_format) {}

template <c_pixel T>
ImageView<T>::ImageView(T *pixels, int2 size, int stride, VFormat format)
	: m_pixels(pixels), m_size(size), m_stride(stride), m_format(format) {
	DASSERT(stride > 0 && size.x >= 0 && size.y >= 0);
	DASSERT(unitByteSize(format) == sizeof(T));
}

template <c_pixel T>
ImageView<T>::ImageView() : m_pixels(nullptr), m_stride(1), m_format(VFormat::rgba8_unorm) {}

template <c_pixel T> inline bool ImageView<T>::inRange(int x, int y) const {
	return x >= 0 && y >= 0 && x < m_size.x && y < m_size.y;
}

template <c_pixel T> inline Span<T> ImageView<T>::row(int y) {
	DASSERT(y >= 0 && y < m_size.y);
	return {&m_pixels[y * m_stride], m_size.x};
}

template <c_pixel T> inline CSpan<T> ImageView<T>::row(int y) const {
	DASSERT(y >= 0 && y < m_size.y);
	return {&m_pixels[y * m_stride], m_size.x};
}

template <c_pixel T> ImageIter<const T> ImageView<T>::begin() const {
	return {m_pixels, m_size.x, m_stride};
}
template <c_pixel T> ImageIter<T> ImageView<T>::begin() { return {m_pixels, m_size.x, m_stride}; }

template <c_pixel T> ImageIter<const T> ImageView<T>::end() const {
	return {m_pixels + m_size.y * m_stride, m_size.x, m_stride};
}
template <c_pixel T> ImageIter<T> ImageView<T>::end() {
	return {m_pixels + m_size.y * m_stride, m_size.x, m_stride};
}

template <c_pixel T> inline T &ImageView<T>::operator()(int2 pos) {
	PASSERT(inRange(pos.x, pos.y));
	return m_pixels[pos.x + pos.y * m_stride];
}

template <c_pixel T> inline const T &ImageView<T>::operator()(int2 pos) const {
	PASSERT(inRange(pos.x, pos.y));
	return m_pixels[pos.x + pos.y * m_stride];
}

template <c_pixel T> inline T &ImageView<T>::operator()(int x, int y) {
	PASSERT(inRange(x, y));
	return m_pixels[x + y * m_stride];
}

template <c_pixel T> inline const T &ImageView<T>::operator()(int x, int y) const {
	PASSERT(inRange(x, y));
	return m_pixels[x + y * m_stride];
}
}
