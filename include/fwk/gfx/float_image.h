// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/image.h"

namespace fwk {

// 2D RGBA float-based image
// Used FColor class to represent pixel values
class FloatImage {
  public:
	explicit FloatImage(int2, FColor fill_color = ColorId::black);
	FloatImage(int2, NoInitTag);
	FloatImage(PodVector<FColor>, int2);
	FloatImage(const Image &, bool srgb_to_linear);
	FloatImage();
	~FloatImage();

	void resize(int2, Maybe<FColor> fill_color = FColor(ColorId::black));
	void premultiplyAlpha();
	void clear();

	// TODO: option to fill a rectangle
	void fill(FColor);

	void blit(const FloatImage &src, int2 target_pos);

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }
	int pixelCount() const { return m_size.x * m_size.y; }

	bool inRange(int x, int y) const;

	bool empty() const { return m_data.empty(); }
	GlFormat format() const;

	void swap(FloatImage &);

	CSpan<FColor> data() const { return m_data; }
	Span<FColor> data() { return m_data; }

	Span<FColor> row(int y);
	CSpan<FColor> row(int y) const;

	FColor &operator()(int2 pos);
	const FColor &operator()(int2 pos) const;
	FColor &operator()(int x, int y);
	const FColor &operator()(int x, int y) const;

	FColor &operator[](int idx) { return m_data[idx]; }
	const FColor &operator[](int idx) const { return m_data[idx]; }

  private:
	PodVector<FColor> m_data;
	int2 m_size;
};

// -------------------------------------------------------------------------------------------
// ---  Inlined code -------------------------------------------------------------------------

inline bool FloatImage::inRange(int x, int y) const {
	return x >= 0 && y >= 0 && x < m_size.x && y < m_size.y;
}

inline Span<FColor> FloatImage::row(int y) {
	DASSERT(y >= 0 && y < m_size.y);
	return {&m_data[y * m_size.x], m_size.x};
}
inline CSpan<FColor> FloatImage::row(int y) const {
	DASSERT(y >= 0 && y < m_size.y);
	return {&m_data[y * m_size.x], m_size.x};
}

inline FColor &FloatImage::operator()(int2 pos) {
	PASSERT(inRange(pos.x, pos.y));
	return m_data[pos.x + pos.y * m_size.x];
}
inline const FColor &FloatImage::operator()(int2 pos) const {
	PASSERT(inRange(pos.x, pos.y));
	return m_data[pos.x + pos.y * m_size.x];
}

inline FColor &FloatImage::operator()(int x, int y) {
	PASSERT(inRange(x, y));
	return m_data[x + y * m_size.x];
}
inline const FColor &FloatImage::operator()(int x, int y) const {
	PASSERT(inRange(x, y));
	return m_data[x + y * m_size.x];
}

}
