// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/texture.h"

#include "fwk/gfx/gl_format.h"
#include "fwk/io/file_stream.h"
#include "fwk/io/file_system.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"

#define STB_DXT_STATIC
#define STB_DXT_IMPLEMENTATION

#include "../extern/stb_dxt.h"

namespace fwk {

namespace detail {
	Ex<Texture> loadSTBI(Stream &);
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

bool Texture::isOpaque() const {
	for(auto &col : m_data)
		if(col.a != 255)
			return false;
	return true;
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

Ex<Texture> Texture::load(Stream &sr, FileType type) { return detail::loadSTBI(sr); }

Ex<Texture> Texture::load(Stream &sr, Str extension) {
	string ext = toLower(extension);
	if(auto type = maybeFromString<FileType>(ext))
		return load(sr, *type);
	if(ext == "jpeg")
		return load(sr, FileType::jpg);

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

GlFormat BlockTexture::glFormat(Type type) {
	return type == Type::dxt1 ? GlFormat::dxt1 : GlFormat::dxt5;
}

int BlockTexture::dataSize(Type type, int2 size) {
	return evalImageSize(glFormat(type), size.x, size.y);
}

BlockTexture::BlockTexture(PodVector<u8> data, int2 size, Type type)
	: m_data(move(data)), m_size(size), m_type(type) {
	DASSERT(m_data.size() >= dataSize(type, m_size));
}

BlockTexture::BlockTexture(const Texture &input, Type type) : m_size(input.size()), m_type(type) {
	m_data.resize(dataSize(type, m_size));
	IColor input_data[4 * 4];
	int bh = (m_size.y + 3) / 4, bw = (m_size.x + 3) / 4;
	u8 *dst = m_data.data();

	for(int by = 0; by < bh; by++) {
		int sy = min(m_size.y - by * 4, 4);
		fill(input_data, ColorId::black);
		for(int bx = 0; bx < bw; bx++) {
			int sx = min(m_size.x - bx * 4, 4);
			const IColor *block_src = &input(bx * 4, by * 4);
			for(int y = 0; y < sy; y++)
				copy(input_data + y * 4, cspan(block_src + m_size.x * y, sx));

			stb_compress_dxt_block(dst, reinterpret_cast<const u8 *>(input_data),
								   m_type == Type::dxt5, STB_DXT_HIGHQUAL);
			dst += m_type == Type::dxt5 ? 16 : 8;
		}
	}
}

BlockTexture::~BlockTexture() = default;

GlFormat BlockTexture::glFormat() const { return glFormat(m_type); }
}
