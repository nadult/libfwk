/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include <cstring>

// Yeah, there are no external symbols in those files, only loaders
// and for some reason loaders won't be registered... (linker optimization?)
#include "gfx/texture_bmp.cpp"
#include "gfx/texture_png.cpp"

namespace fwk {

Texture::Texture() : m_size(0, 0) {}
Texture::Texture(int w, int h) : m_data(w * h), m_size(w, h) {}
Texture::Texture(Stream &sr) : Texture() { load(sr); }

void Texture::resize(int w, int h) {
	m_size = int2(w, h);
	m_data.resize(w * h);
}

void Texture::swap(Texture &tex) {
	std::swap(m_size, tex.m_size);
	m_data.swap(tex.m_data);
}

void Texture::clear() {
	m_size = int2(0, 0);
	m_data.clear();
}

void Texture::fill(Color color) {
	for(int n = 0; n < m_data.size(); n++)
		m_data[n] = color;
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

	blit_size = min(blit_size, size() - dst_pos);
	if(blit_size.x <= 0 || blit_size.y <= 0)
		return;

	for(int y = 0; y < blit_size.y; y++) {
		Color const *src_line = src.line(src_pos.y + y) + src_pos.x;
		Color *dst_line = line(dst_pos.y + y) + dst_pos.x;
		memcpy(dst_line, src_line, sizeof(Color) * blit_size.x);
	}
}

bool Texture::testPixelAlpha(const int2 &pos) const {
	if(pos.x < 0 || pos.y < 0 || pos.x >= m_size.x || pos.y >= m_size.y)
		return false;

	return (*this)(pos.x, pos.y).a > 0;
}

// TODO: make it thread safe
using TextureLoaders = vector<pair<string, Texture::Loader>>;

static TextureLoaders &loaders() {
	static TextureLoaders s_loaders;
	return s_loaders;
}

void Texture::load(Stream &sr) {
	string file_name = sr.name();
	auto dot_pos = file_name.rfind('.');
	string ext = toLower(dot_pos == string::npos ? string() : file_name.substr(dot_pos + 1));

	Texture::Loader loader = nullptr;
	for(const auto &it : loaders())
		if(it.first == ext) {
			loader = it.second;
			break;
		}

	if(!loader)
		THROW("Extension '%s' is not supported", ext.c_str());

	loader(sr, m_data, m_size);
	DASSERT(m_size.x >= 0 && m_size.y >= 0);
}

Texture::RegisterLoader::RegisterLoader(const char *ext, Loader func) {
	DASSERT(toLower(ext) == ext);
	loaders().emplace_back(make_pair(ext, func));
}

void Texture::save(Stream &sr) const { saveTGA(sr); }
}
