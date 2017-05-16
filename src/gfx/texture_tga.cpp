/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include <cstring>

namespace fwk {

struct Header {
	Header() { memset(this, 0, sizeof(Header)); }

	void save(Stream &sr) const {
		sr.pack(id_length, color_map_type, data_type_code, color_map_origin, color_map_length,
				color_map_depth, x_origin, y_origin, width, height, bits_per_pixel,
				image_descriptor);
	}

	void load(Stream &sr) {
		sr.unpack(id_length, color_map_type, data_type_code, color_map_origin, color_map_length,
				  color_map_depth, x_origin, y_origin, width, height, bits_per_pixel,
				  image_descriptor);
	}

	u8 id_length;
	u8 color_map_type;
	u8 data_type_code;
	u16 color_map_origin;
	u16 color_map_length;
	u8 color_map_depth;
	u16 x_origin;
	u16 y_origin;
	u16 width;
	u16 height;
	u8 bits_per_pixel;
	u8 image_descriptor;
};

namespace {
	void loadTGA(Stream &sr, PodArray<IColor> &out_data, int2 &out_size) {
		Header hdr;
		enum { max_width = 2048 };

		sr >> hdr;

		if(hdr.data_type_code != 2)
			THROW("Only uncompressed RGB data type is supported (id:%d)", (int)hdr.data_type_code);
		if(hdr.bits_per_pixel != 24 && hdr.bits_per_pixel != 32)
			THROW("Only 24 and 32-bit tga files are supported (bpp:%d)", (int)hdr.bits_per_pixel);
		if(hdr.width > max_width)
			THROW("Bitmap is too wide (%d pixels): max width is %d", (int)hdr.width,
				  (int)max_width);

		sr.seek(sr.pos() + hdr.id_length);

		unsigned bpp = hdr.bits_per_pixel / 8;
		out_data.resize(hdr.width * hdr.height);
		out_size = int2(hdr.width, hdr.height);

		bool v_flip = hdr.image_descriptor & 16;
		bool h_flip = hdr.image_descriptor & 32;
		ASSERT(!v_flip && !h_flip && "TODO");

		if(bpp == 3) {
			for(int y = hdr.height - 1; y >= 0; y--) {
				unsigned char line[max_width * 3];
				sr.loadData(line, hdr.width * 3);
				IColor *dst = &out_data[y * hdr.width];
				for(int x = 0; x < hdr.width; x++)
					dst[x] = IColor(line[x * 3 + 0], line[x * 3 + 1], line[x * 3 + 2]);
			}
		} else if(bpp == 4) {
			for(int y = hdr.height - 1; y >= 0; y--) {
				IColor *colors = &out_data[y * hdr.width];
				sr.loadData(colors, hdr.width * 4);
				for(int x = 0; x < hdr.width; x++)
					colors[x] = colors[x].bgra();
			}
		}
	}
	Texture::RegisterLoader tga_loader("tga", loadTGA);
}

void Texture::saveTGA(Stream &sr) const {
	Header header;
	memset(&header, 0, sizeof(header));

	header.data_type_code = 2;
	header.color_map_depth = 32;
	header.width = m_size.x;
	header.height = m_size.y;
	header.bits_per_pixel = 32;
	header.image_descriptor = 8;

	sr << header;
	vector<IColor> line(m_size.x);
	for(int y = m_size.y - 1; y >= 0; y--) {
		memcpy(&line[0], this->line(y), m_size.x * sizeof(IColor));
		for(int x = 0; x < m_size.x; x++)
			line[x] = IColor(line[x].b, line[x].g, line[x].r, line[x].a);
		sr.saveData(&line[0], m_size.x * sizeof(IColor));
	}
}
}
