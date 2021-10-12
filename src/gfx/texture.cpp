// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/texture.h"

#include "fwk/gfx/gl_format.h"
#include "fwk/io/file_stream.h"
#include "fwk/io/file_system.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"

namespace fwk {

namespace detail {

	Ex<Texture> loadBMP(Stream &);
	Ex<Texture> loadPNG(Stream &);
	Ex<Texture> loadTGA(Stream &);
}

Texture::Texture() {}
Texture::Texture(int2 size) : m_data(size.x * size.y), m_size(size) {
	DASSERT(size.x >= 0 && size.y >= 0);
}

Texture::Texture(PodVector<IColor> data, int2 size) : m_data(move(data)), m_size(size) {
	DASSERT(size.x >= 0 && size.y >= 0);
	DASSERT(m_data.size() >= size.x * size.y);
}

GlFormat Texture::format() const { return GlFormat::rgba; }

void Texture::resize(int2 size, Maybe<IColor> fill_color) {
	if(size == m_size)
		return;
	Texture new_tex(size);
	if(fill_color)
		new_tex.fill(*fill_color);
	new_tex.blit(*this, int2(0, 0));
	swap(new_tex);
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

Ex<Texture> Texture::load(ZStr file_name, Maybe<FileType> type) {
	auto loader = EX_PASS(fileLoader(file_name));
	if(type)
		return load(loader, *type);
	return load(loader);
	if(auto ext = fileNameExtension(file_name))
		return load(loader, *ext);
	return ERROR("File '%' has no extension: don't know which loader to use", file_name);
}

Ex<Texture> Texture::load(Stream &sr, FileType type) {
	switch(type) {
	case FileType::tga:
		return detail::loadTGA(sr);
	case FileType::bmp:
		return detail::loadBMP(sr);
	case FileType::png:
		return detail::loadPNG(sr);
	}
}

Ex<Texture> Texture::load(Stream &sr, Str extension) {
	string ext = toLower(extension);
	if(auto type = maybeFromString<FileType>(extension))
		return load(sr, *type);
	for(auto &loader : loaders())
		if(loader.first == extension)
			return loader.second(sr);
	return ERROR("Extension '%' not supported", extension);
}

Ex<Texture> Texture::load(FileStream &sr) {
	if(auto ext = fileNameExtension(sr.name()))
		return load(sr, *ext);
	return ERROR("File '%' has no extension: don't know which loader to use", sr.name());
}

Texture::RegisterLoader::RegisterLoader(const char *ext, Loader func) {
	DASSERT(toLower(ext) == ext);
	loaders().emplace_back(ext, func);
}
}
