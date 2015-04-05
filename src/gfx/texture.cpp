/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include <cstring>

namespace fwk {
namespace gfx {

	Texture::Texture(int w, int h) : m_data(w * h), m_width(w), m_height(h) {}

	Texture::Texture() : m_width(0), m_height(0) {}

	void Texture::resize(int w, int h) {
		m_width = w;
		m_height = h;
		m_data.resize(m_width * m_height);
	}

	void Texture::swap(Texture &tex) {
		std::swap(m_width, tex.m_width);
		std::swap(m_height, tex.m_height);
		m_data.swap(tex.m_data);
	}

	void Texture::clear() {
		m_width = m_height = 0;
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
		if(pos.x < 0 || pos.y < 0 || pos.x >= m_width || pos.y >= m_height)
			return false;

		return (*this)(pos.x, pos.y).a > 0;
	}

	void Texture::load(Stream &sr) {
		string fileName = sr.name(), ext;
		{
			ext = fileName.substr(std::max(size_t(0), fileName.length() - 4));
			for(int n = 0, end = ext.length(); n < end; n++)
				ext[n] = tolower(ext[n]);
		}

		if(sr.isLoading()) {
			if(ext == ".png")
				loadPNG(sr);
			else if(ext == ".tga")
				loadTGA(sr);
			else
				THROW("%s format is not supported (Only BMP, TGA and PNG for now)", ext.c_str());
		}
	}

	void Texture::save(Stream &sr) const { saveTGA(sr); }
}
}
