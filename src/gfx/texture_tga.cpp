/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include <cstring>

namespace fwk {

struct Header {
	u8 idlength;
	u8 colourmaptype;
	u8 datatypecode;
	u16 colourmaporigin;
	u16 colourmaplength;
	u8 colourmapdepth;
	u16 x_origin;
	u16 y_origin;
	u16 width;
	u16 height;
	u8 bitsperpixel;
	u8 imagedescriptor;
} __attribute__((packed));

namespace {
	void loadTGA(Stream &sr, PodArray<Color> &out_data, int2 &out_size) {
		Header hdr;
		enum { max_width = 2048 };

		sr.loadData(&hdr, sizeof(hdr));

		if(hdr.datatypecode != 2)
			THROW("Only uncompressed RGB data type is supported (id:%d)", (int)hdr.datatypecode);
		if(hdr.bitsperpixel != 24 && hdr.bitsperpixel != 32)
			THROW("Only 24 and 32-bit tga files are supported (bpp:%d)", (int)hdr.bitsperpixel);
		if(hdr.width > max_width)
			THROW("Bitmap is too wide (%d pixels): max width is %d", (int)hdr.width,
				  (int)max_width);

		sr.seek(sr.pos() + hdr.idlength);

		unsigned bpp = hdr.bitsperpixel / 8;
		out_data.resize(hdr.width * hdr.height);
		out_size = int2(hdr.width, hdr.height);

		bool v_flip = hdr.imagedescriptor & 16;
		bool h_flip = hdr.imagedescriptor & 32;
		ASSERT(!v_flip && !h_flip && "TODO");

		if(bpp == 3) {
			for(int y = hdr.height - 1; y >= 0; y--) {
				char line[max_width * 3];
				sr.loadData(line, hdr.width * 3);
				Color *dst = &out_data[y * hdr.width];
				for(int x = 0; x < hdr.width; x++)
					dst[x] = Color(line[x * 3 + 0], line[x * 3 + 1], line[x * 3 + 2]);
			}
		} else if(bpp == 4) {
			for(int y = hdr.height - 1; y >= 0; y--)
				sr.loadData(&out_data[y * hdr.width], hdr.width * 4);
		}
	}
	Texture::RegisterLoader tga_loader("tga", loadTGA);
}

void Texture::saveTGA(Stream &sr) const {
	Header header;
	memset(&header, 0, sizeof(header));

	header.datatypecode = 2;
	header.colourmapdepth = 32;
	header.width = m_size.x;
	header.height = m_size.y;
	header.bitsperpixel = 32;
	header.imagedescriptor = 8;

	sr << header;
	vector<Color> line(m_size.x);
	for(int y = m_size.y - 1; y >= 0; y--) {
		memcpy(&line[0], this->line(y), m_size.x * sizeof(Color));
		for(int x = 0; x < m_size.x; x++)
			line[x] = Color(line[x].b, line[x].g, line[x].r, line[x].a);
		sr.saveData(&line[0], m_size.x * sizeof(Color));
	}
}
}
