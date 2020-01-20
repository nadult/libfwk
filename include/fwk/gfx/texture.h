// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/math_base.h"
#include "fwk/pod_vector.h"
#include "fwk/sys_base.h"

namespace fwk {

struct HeightMap16bit {
  public:
	static Ex<HeightMap16bit> load(Stream &);

	vector<u16> data;
	int2 size;
};

DEFINE_ENUM(TextureFileType, tga, png, bmp);

// simple RGBA32 texture
class Texture {
  public:
	using FileType = TextureFileType;
	// TODO: make it initialize with black or something by default
	explicit Texture(int2);
	Texture(PodVector<IColor>, int2);
	Texture();

	// Loading from TGA, BMP, PNG, DDS
	// TODO: loading from memory (through DataStream or something)
	static Ex<Texture> load(ZStr file_name, Maybe<FileType> = none);
	static Ex<Texture> load(FileStream &, Maybe<FileType>);

	using Loader = Ex<Texture> (*)(Stream &);
	struct RegisterLoader {
		RegisterLoader(const char *locase_ext, Loader);
	};

	void resize(int2, Maybe<IColor> fill = IColor(ColorId::black));
	void clear();
	void fill(IColor);
	void blit(const Texture &src, int2 target_pos);

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }
	int pixelCount() const { return m_size.x * m_size.y; }

	bool empty() const { return m_data.empty(); }
	bool testPixelAlpha(const int2 &) const;
	GlFormat format() const;

	void swap(Texture &);

	Ex<void> saveTGA(Stream &) const;
	Ex<void> saveTGA(ZStr file_name) const;

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
	PodVector<IColor> m_data;
	int2 m_size;
};
}
