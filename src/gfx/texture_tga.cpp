// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/texture.h"

#include "fwk/io/file_stream.h"
#include "fwk/io/stream.h"
#include "fwk/sys/expected.h"

namespace fwk {

struct EXCEPT TGAHeader {
	TGAHeader() { memset(this, 0, sizeof(TGAHeader)); }

	void save(Stream &sr) const {
		// TODO: this can be automated with structure binding
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

Ex<void> Texture::saveTGA(Stream &sr) const {
	TGAHeader header;

	header.data_type_code = 2;
	header.color_map_depth = 32;
	header.width = m_size.x;
	header.height = m_size.y;
	header.bits_per_pixel = 32;
	header.image_descriptor = 8;

	header.save(sr);
	vector<IColor> line(m_size.x);
	for(int y = m_size.y - 1; y >= 0; y--) {
		memcpy(&line[0], this->line(y), m_size.x * sizeof(IColor));
		for(int x = 0; x < m_size.x; x++)
			line[x] = IColor(line[x].b, line[x].g, line[x].r, line[x].a);
		sr.saveData(cspan(&line[0], m_size.x).reinterpret<char>());
	}

	return {};
}

Ex<void> Texture::saveTGA(ZStr file_name) const {
	auto file = fileSaver(file_name);
	return file ? saveTGA(*file) : file.error();
}
}
