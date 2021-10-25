// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx/color.h"
#include "fwk/math_base.h"
#include "fwk/pod_vector.h"
#include "fwk/sys_base.h"

namespace fwk {

DEFINE_ENUM(ImageFileType, tga, png, bmp, jpg, gif, pgm, ppm);
DEFINE_ENUM(ImageRescaleOpt, srgb, premultiplied_alpha);
using ImageRescaleOpts = EnumFlags<ImageRescaleOpt>;

// 2D RGBA 8-bit per channel image
// Used IColor class to represent pixel values
class Image {
  public:
	using FileType = ImageFileType;

	explicit Image(int2, IColor fill_color = ColorId::black);
	Image(int2, NoInitTag);
	Image(PodVector<IColor>, int2);
	Image(const FloatImage &, bool linear_to_srgb);
	Image();
	~Image();

	// Loading from supported file types
	// TODO: loading from memory (through DataStream or something)
	static Ex<Image> load(ZStr file_name, Maybe<FileType> = none);
	static Ex<Image> load(Stream &, FileType);
	static Ex<Image> load(Stream &, Str extension);
	static Ex<Image> load(FileStream &);

	using Loader = Ex<Image> (*)(Stream &);
	struct RegisterLoader {
		RegisterLoader(const char *locase_ext, Loader);
	};

	void resize(int2, Maybe<IColor> fill = IColor(ColorId::black));
	Image rescale(int2 new_size, ImageRescaleOpts opts = none) const;

	void clear();
	void fill(IColor);

	void blit(const Image &src, int2 target_pos);

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }
	int pixelCount() const { return m_size.x * m_size.y; }

	bool inRange(int x, int y) const;

	bool empty() const { return m_data.empty(); }
	bool testPixelAlpha(const int2 &) const;
	bool isOpaque() const;
	GlFormat format() const;

	void swap(Image &);

	Ex<> saveTGA(Stream &) const;
	Ex<> saveTGA(ZStr file_name) const;

	CSpan<IColor> data() const { return m_data; }
	Span<IColor> data() { return m_data; }

	Span<IColor> row(int y);
	CSpan<IColor> row(int y) const;

	IColor &operator()(int2 pos);
	const IColor &operator()(int2 pos) const;

	IColor &operator()(int x, int y);
	const IColor &operator()(int x, int y) const;

	IColor &operator[](int idx) { return m_data[idx]; }
	const IColor &operator[](int idx) const { return m_data[idx]; }

  private:
	PodVector<IColor> m_data;
	int2 m_size;
};

// -------------------------------------------------------------------------------------------
// ---  Inlined code -------------------------------------------------------------------------

inline bool Image::inRange(int x, int y) const {
	return x >= 0 && y >= 0 && x < m_size.x && y < m_size.y;
}

inline Span<IColor> Image::row(int y) {
	DASSERT(y >= 0 && y < m_size.y);
	return {&m_data[y * m_size.x], m_size.x};
}
inline CSpan<IColor> Image::row(int y) const {
	DASSERT(y >= 0 && y < m_size.y);
	return {&m_data[y * m_size.x], m_size.x};
}

inline IColor &Image::operator()(int2 pos) {
	PASSERT(inRange(pos.x, pos.y));
	return m_data[pos.x + pos.y * m_size.x];
}
inline const IColor &Image::operator()(int2 pos) const {
	PASSERT(inRange(pos.x, pos.y));
	return m_data[pos.x + pos.y * m_size.x];
}

inline IColor &Image::operator()(int x, int y) {
	PASSERT(inRange(x, y));
	return m_data[x + y * m_size.x];
}
inline const IColor &Image::operator()(int x, int y) const {
	PASSERT(inRange(x, y));
	return m_data[x + y * m_size.x];
}

}
