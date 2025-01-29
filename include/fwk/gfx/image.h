// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx/image_view.h"
#include "fwk/math_base.h"
#include "fwk/pod_vector.h"
#include "fwk/sys_base.h"
#include "fwk/vulkan_base.h"

namespace fwk {

DEFINE_ENUM(ImageFileType, tga, png, bmp, jpg, gif, pgm, ppm);
DEFINE_ENUM(ImageRescaleOpt, srgb, premultiplied_alpha);
using ImageRescaleOpts = EnumFlags<ImageRescaleOpt>;

class Image {
  public:
	explicit Image(int2 size, VFormat = VFormat::rgba8_unorm);
	Image(Image, VFormat new_format);
	Image(int2 size, NoInitTag, VFormat = VFormat::rgba8_unorm);
	Image(int2 size, IColor fill, VFormat = VFormat::rgba8_unorm);
	Image(PodVector<u8>, int2 size, VFormat);

	template <c_pixel T> Image(PodVector<T> data, int2 size, VFormat format);
	template <c_pixel T> Image(int2 size, T fill, VFormat format);
	Image();
	~Image();

	void clear();
	void swap(Image &);

	// Loading from supported file types
	// TODO: loading from memory (through DataStream or something)
	static Ex<Image> load(ZStr file_name, Maybe<ImageFileType> = none);
	static Ex<Image> load(Stream &, ImageFileType);
	static Ex<Image> load(Stream &, Str extension);
	static Ex<Image> load(FileStream &);

	// Supported formats: RGBA8
	Ex<> saveTGA(Stream &) const;
	Ex<> saveTGA(ZStr file_name) const;

	using Loader = Ex<Image> (*)(Stream &);
	struct RegisterLoader {
		RegisterLoader(const char *locase_ext, Loader);
	};

	template <c_pixel TPixel> void fill(const TPixel &);
	void blit(const Image &src, int2 target_pos = {});

	void resize(int2, Maybe<IColor> fill = IColor(ColorId::black));
	Image rescale(int2 new_size, ImageRescaleOpts opts = none) const;

	void setFormat(VFormat new_format);

	static Image compressBC(const Image &, VFormat);

	static int maxMipmapLevels(int max_dimension) { return int(log2(max_dimension)) + 1; }
	static int maxMipmapLevels(int2 size) { return maxMipmapLevels(max(size.x, size.y)); }

	// ---------- Accessors -------------------------------------------------------------

	CSpan<u8> data() const { return m_data; }
	Span<u8> data() { return m_data; }

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }

	VFormat format() const { return m_format; }
	bool empty() const { return m_size.x == 0 || m_size.y == 0; }

	template <c_pixel T> Span<T> row(int y);
	template <c_pixel T> CSpan<T> row(int y) const;

	template <c_pixel T> ImageView<const T> pixels() const;
	template <c_pixel T> ImageView<T> pixels();

  private:
	PodVector<u8> m_data;
	int2 m_size;
	VFormat m_format;
};

// -------------------------------------------------------------------------------------------
// ---  Inlined code -------------------------------------------------------------------------

template <c_pixel T>
Image::Image(PodVector<T> data, int2 size, VFormat format)
	: m_data(std::move(data.template reinterpret<u8>())), m_size(size), m_format(format) {
	DASSERT(sizeof(T) == unitByteSize(format));
	DASSERT(m_data.size() * sizeof(T) >= imageByteSize(format, m_size));
}

template <c_pixel T> Image::Image(int2 size, T value, VFormat format) : Image(size, format) {
	fill(value);
}

template <c_pixel T> void Image::fill(const T &color) {
	DASSERT(sizeof(T) == unitByteSize(m_format));
	for(int y = 0; y < m_size.y; y++)
		fwk::fill(row<T>(y), color);
}

template <c_pixel T> Span<T> Image::row(int y) {
	DASSERT(y >= 0 && y < m_size.y);
	DASSERT(sizeof(T) == unitByteSize(m_format));
	return {reinterpret_cast<T *>(m_data.data()) + y * m_size.x, m_size.x};
}

template <c_pixel T> CSpan<T> Image::row(int y) const {
	DASSERT(y >= 0 && y < m_size.y);
	DASSERT(sizeof(T) == unitByteSize(m_format));
	return {reinterpret_cast<const T *>(m_data.data()) + y * m_size.x, m_size.x};
}

template <c_pixel T> ImageView<const T> Image::pixels() const {
	DASSERT(sizeof(T) == unitByteSize(m_format));
	return {reinterpret_cast<const T *>(m_data.data()), m_size, m_size.x, m_format};
}

template <c_pixel T> ImageView<T> Image::pixels() {
	DASSERT(sizeof(T) == unitByteSize(m_format));
	return {reinterpret_cast<T *>(m_data.data()), m_size, m_size.x, m_format};
}

}
