// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/image.h"

#include "fwk/gfx/float_image.h"
#include "fwk/io/file_stream.h"
#include "fwk/io/file_system.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"

namespace fwk {

namespace detail {
	Ex<Image> loadSTBI(Stream &);
}

Image::Image() = default;
Image::Image(int2 size, NoInitTag) : m_data(size.x * size.y), m_size(size) {
	DASSERT(size.x >= 0 && size.y >= 0);
}

Image::Image(int2 size, IColor fill_color) : Image(size, no_init) { fill(fill_color); }

Image::Image(PodVector<IColor> data, int2 size) : m_data(move(data)), m_size(size) {
	DASSERT(size.x >= 0 && size.y >= 0);
	DASSERT(m_data.size() >= size.x * size.y);
}

Image::Image(const FloatImage &rhs, bool linear_to_srgb)
	: m_data(rhs.pixelCount()), m_size(rhs.size()) {
	for(int y = 0; y < m_size.y; y++) {
		auto src = rhs.row(y);
		auto dst = row(y);
		if(linear_to_srgb) {
			linearToSrgb(src, dst);
		} else {
			// TODO: clamping optional ?
			for(int x = 0; x < m_size.x; x++)
				dst[x] = IColor(src[x]);
		}
	}
}

Image::~Image() = default;

void Image::resize(int2 size, Maybe<IColor> fill_color) {
	if(size == m_size)
		return;
	Image new_tex(size, no_init);
	if(fill_color)
		new_tex.fill(*fill_color);
	new_tex.blit(*this, int2(0, 0));
	swap(new_tex);
}

void Image::swap(Image &tex) {
	std::swap(m_size, tex.m_size);
	m_data.swap(tex.m_data);
}

void Image::clear() {
	m_size = int2();
	m_data.clear();
}

void Image::fill(IColor color) {
	for(auto &col : m_data)
		col = color;
}

void Image::blit(Image const &src, int2 dst_pos) {
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
		auto src_row = src.row(src_pos.y + y).subSpan(src_pos.x);
		auto dst_row = row(dst_pos.y + y).subSpan(dst_pos.x, dst_pos.x + blit_size.x);
		copy(dst_row, src_row);
	}
}

bool Image::testPixelAlpha(const int2 &pos) const {
	if(pos.x < 0 || pos.y < 0 || pos.x >= m_size.x || pos.y >= m_size.y)
		return false;

	return (*this)(pos.x, pos.y).a > 0;
}

bool Image::isOpaque() const {
	for(auto &col : m_data)
		if(col.a != 255)
			return false;
	return true;
}

using ImageLoaders = vector<Pair<string, Image::Loader>>;
static ImageLoaders &loaders() {
	static ImageLoaders s_loaders;
	return s_loaders;
}

Ex<Image> Image::load(ZStr file_name, Maybe<FileType> type) {
	auto loader = EX_PASS(fileLoader(file_name));
	if(type)
		return load(loader, *type);
	return load(loader);
	if(auto ext = fileNameExtension(file_name))
		return load(loader, *ext);
	return ERROR("File '%' has no extension: don't know which loader to use", file_name);
}

Ex<Image> Image::load(Stream &sr, FileType type) { return detail::loadSTBI(sr); }

Ex<Image> Image::load(Stream &sr, Str extension) {
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

Ex<Image> Image::load(FileStream &sr) {
	if(auto ext = fileNameExtension(sr.name()))
		return load(sr, *ext);
	return ERROR("File '%' has no extension: don't know which loader to use", sr.name());
}

Image::RegisterLoader::RegisterLoader(const char *ext, Loader func) {
	DASSERT(toLower(ext) == ext);
	loaders().emplace_back(ext, func);
}
}
