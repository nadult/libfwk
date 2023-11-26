// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/image.h"

#include "fwk/io/file_stream.h"
#include "fwk/io/file_system.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"

#define STB_DXT_STATIC
#define STB_DXT_IMPLEMENTATION

#include "../extern/stb_dxt.h"

// TODO: efficient accessor for all pixels in an image
// TODO: subImage accessor

namespace fwk {

namespace detail {
	Ex<Image> loadSTBI(Stream &);
}

Image::Image(int2 size, VFormat format)
	: m_data(imageByteSize(format, size)), m_size(size), m_format(format) {
	fwk::fill(m_data, 0);
	DASSERT(size.x >= 0 && size.y >= 0);
}

Image::Image(int2 size, NoInitTag, VFormat format)
	: m_data(imageByteSize(format, size)), m_size(size), m_format(format) {
	DASSERT(size.x >= 0 && size.y >= 0);
}

Image::Image(int2 size, IColor color, VFormat format)
	: m_data(imageByteSize(format, size)), m_size(size), m_format(format) {
	DASSERT(size.x >= 0 && size.y >= 0);
	fill(color);
}

Image::Image(PodVector<u8> data, int2 size, VFormat format)
	: m_data(std::move(data)), m_size(size), m_format(format) {
	DASSERT(size.x >= 0 && size.y >= 0);
	DASSERT(m_data.size() >= imageByteSize(format, m_size));
}

Image::Image(Image rhs, VFormat format)
	: m_data(std::move(rhs.m_data)), m_size(rhs.m_size), m_format(format) {
	rhs.m_size = int2(0, 0);
	DASSERT(areCompatible(rhs.m_format, format));
}

Image::Image() : m_format(VFormat::rgba8_unorm) {}
Image::~Image() = default;

using ImageLoaders = vector<Pair<string, Image::Loader>>;
static ImageLoaders &loaders() {
	static ImageLoaders s_loaders;
	return s_loaders;
}

Ex<Image> Image::load(ZStr file_name, Maybe<ImageFileType> type) {
	auto loader = EX_PASS(fileLoader(file_name));
	if(type)
		return load(loader, *type);
	return load(loader);
	if(auto ext = fileNameExtension(file_name))
		return load(loader, *ext);
	return ERROR("File '%' has no extension: don't know which loader to use", file_name);
}

Ex<Image> Image::load(Stream &sr, ImageFileType type) { return detail::loadSTBI(sr); }

Ex<Image> Image::load(Stream &sr, Str extension) {
	string ext = toLower(extension);
	if(auto type = maybeFromString<ImageFileType>(ext))
		return load(sr, *type);
	if(ext == "jpeg")
		return load(sr, ImageFileType::jpg);

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

Image Image::compressBC(const Image &image, VFormat format) {
	auto base_format = baseFormat(format);
	auto numeric_format = numericFormat(format);

	DASSERT(isOneOf(base_format, VBaseFormat::bc1_rgb, VBaseFormat::bc3_rgba));
	DASSERT(baseFormat(image.format()) == VBaseFormat::rgba8);
	// TODO: add support for bc4 & bc5

	PodVector<u8> data(imageByteSize(format, image.size()));
	// TODO: better naming
	int2 block_size = imageBlockSize(format, image.size());
	int block_byte_size = unitByteSize(format);
	int w = image.width(), h = image.height();

	DASSERT(unitSize(format) == 4);
	auto pixels = image.pixels<IColor>();
	auto *dst = data.data();
	for(int by = 0; by < block_size.y; by++) {
		IColor input_data[4 * 4];
		int sy = min(h - by * 4, 4);
		fwk::fill(input_data, ColorId::black);
		for(int bx = 0; bx < block_size.x; bx++) {
			int sx = min(w - bx * 4, 4);

			for(int y = 0; y < sy; y++)
				copy(input_data + y * 4, cspan(&pixels(bx * 4, by * 4 + y), sx));

			stb_compress_dxt_block(dst, reinterpret_cast<const u8 *>(input_data),
								   base_format == VBaseFormat::bc3_rgba, STB_DXT_HIGHQUAL);
			dst += block_byte_size;
		}
	}

	return {std::move(data), image.size(), format};
}

void Image::setFormat(VFormat new_format) {
	DASSERT(areCompatible(m_format, new_format));
	m_format = new_format;
}

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

void Image::blit(const Image &src, int2 dst_pos) {
	DASSERT(areCompatible(m_format, src.m_format));

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

	int pixel_size = unitByteSize(m_format);

	const auto *src_data = src.m_data.data() + (src_pos.y * src.m_size.x + src_pos.x) * pixel_size;
	auto *dst_data = m_data.data() + (dst_pos.y * m_size.x + dst_pos.x) * pixel_size;
	for(int y = 0; y < blit_size.y; y++) {
		memcpy(dst_data, src_data, blit_size.x * pixel_size);
		src_data += src.m_size.x * pixel_size;
		dst_data += m_size.x * pixel_size;
	}
}
}
