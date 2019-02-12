// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/texture.h"
#include "fwk/str.h"
#include "fwk/sys/stream.h"

namespace fwk {

namespace detail {

	void loadBMP(Stream &, PodVector<IColor> &, int2 &);
	void loadPNG(Stream &, PodVector<IColor> &, int2 &);
	void loadTGA(Stream &, PodVector<IColor> &, int2 &);
}

Texture::Texture() {}
Texture::Texture(int2 size) : m_data(size.x * size.y), m_size(size) {}
Texture::Texture(Stream &sr, Maybe<FileType> ft) : Texture() { load(sr, ft); }

GlFormat Texture::format() const { return GlFormat::rgba; }

void Texture::resize(int2 size) {
	m_size = size;
	m_data.resize(size.x * size.y);
}

void Texture::swap(Texture &tex) {
	std::swap(m_size, tex.m_size);
	m_data.swap(tex.m_data);
}

void Texture::clear() {
	m_size = int2();
	m_data.clear();
}

void Texture::fill(IColor color) {
	for(auto &col : m_data)
		col = color;
}

void Texture::blit(Texture const &src, int2 dst_pos) {
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

	for(int y = 0; y < blit_size.y; y++) {
		IColor const *src_line = src.line(src_pos.y + y) + src_pos.x;
		IColor *dst_line = line(dst_pos.y + y) + dst_pos.x;
		memcpy(dst_line, src_line, sizeof(IColor) * blit_size.x);
	}
}

bool Texture::testPixelAlpha(const int2 &pos) const {
	if(pos.x < 0 || pos.y < 0 || pos.x >= m_size.x || pos.y >= m_size.y)
		return false;

	return (*this)(pos.x, pos.y).a > 0;
}

using TextureLoaders = vector<Pair<string, Texture::Loader>>;
static TextureLoaders &loaders() {
	static TextureLoaders s_loaders;
	return s_loaders;
}

void Texture::load(Stream &sr, Maybe<FileType> ft) {
	string file_name = sr.name();
	auto dot_pos = file_name.rfind('.');

	if(!ft) {
		string ext = toLower(dot_pos == string::npos ? string() : file_name.substr(dot_pos + 1));
		if(ext.empty())
			FATAL("No extension information: don't know which loader to use");
		ft = tryFromString<FileType>(ext.c_str());
		if(!ft) {
			for(auto &loader : loaders())
				if(loader.first == ext) {
					loader.second(sr, m_data, m_size);
					DASSERT(m_size.x >= 0 && m_size.y >= 0);
					return;
				}

			FATAL("Extension '%s' is not supported", ext.c_str());
		}
	}

	switch(*ft) {
	case FileType::tga:
		detail::loadTGA(sr, m_data, m_size);
		break;

	case FileType::bmp:
		detail::loadBMP(sr, m_data, m_size);
		break;

	case FileType::png:
		detail::loadPNG(sr, m_data, m_size);
		break;
	}

	DASSERT(m_size.x >= 0 && m_size.y >= 0);
}

Texture::RegisterLoader::RegisterLoader(const char *ext, Loader func) {
	DASSERT(toLower(ext) == ext);
	loaders().emplace_back(ext, func);
}

void Texture::save(Stream &sr) const { saveTGA(sr); }
}
