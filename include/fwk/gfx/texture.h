// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/texture_format.h"
#include "fwk_base.h"
#include "fwk_math.h"

namespace fwk {

struct HeightMap16bit {
  public:
	void load(Stream &);

	vector<u16> data;
	int2 size;
};

// simple RGBA32 texture
class Texture {
  public:
	// TODO: make it initialize with black or something by default
	Texture(int2);
	Texture(Stream &);
	Texture();

	// data is not preserved
	// TODO: it should be or else remove this function
	void resize(int2);
	void clear();
	void fill(IColor);
	void blit(const Texture &src, int2 target_pos);

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }
	int pixelCount() const { return m_size.x * m_size.y; }

	bool empty() const { return m_data.empty(); }
	bool testPixelAlpha(const int2 &) const;

	TextureFormat format() const { return TextureFormatId::rgba; }

	// Loading from TGA, BMP, PNG, DDS
	void load(Stream &);
	void save(Stream &) const;
	void swap(Texture &);

	using Loader = void (*)(Stream &, PodArray<IColor> &out_data, int2 &out_size);
	struct RegisterLoader {
		RegisterLoader(const char *locase_ext, Loader);
	};

	void saveTGA(Stream &) const;

	IColor *data() { return m_data.data(); }
	const IColor *data() const { return m_data.data(); }

	IColor *line(int y) {
		DASSERT(y < m_size.y);
		return &m_data[y * m_size.x];
	}
	const IColor *line(int y) const {
		DASSERT(y < m_size.y);
		return &m_data[y * m_size.x];
	}

	IColor &operator()(int x, int y) { return m_data[x + y * m_size.x]; }
	const IColor operator()(int x, int y) const { return m_data[x + y * m_size.x]; }

	IColor &operator[](int idx) { return m_data[idx]; }
	const IColor operator[](int idx) const { return m_data[idx]; }

  private:
	PodArray<IColor> m_data;
	int2 m_size;
};
}