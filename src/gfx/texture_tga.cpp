// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/texture.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/file_stream.h"

namespace fwk {

namespace detail {
	struct TGAHeader {
		TGAHeader() { memset(this, 0, sizeof(TGAHeader)); }

		void save(FileStream &sr) const {
			// TODO: this can be automated with structure binding
			sr.pack(id_length, color_map_type, data_type_code, color_map_origin, color_map_length,
					color_map_depth, x_origin, y_origin, width, height, bits_per_pixel,
					image_descriptor);
		}

		void load(FileStream &sr) {
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

	Expected<Texture> loadTGA(FileStream &sr) {
		TGAHeader hdr;
		enum { max_width = 4096, max_height = 4096 };

		hdr.load(sr);
		EXPECT_NO_ERRORS();

		if(hdr.data_type_code != 2)
			return ERROR("Only uncompressed RGB data type is supported (id:%)", hdr.data_type_code);
		if(hdr.bits_per_pixel != 24 && hdr.bits_per_pixel != 32)
			return ERROR("Only 24 and 32-bit tga files are supported (bpp:%)", hdr.bits_per_pixel);
		if(hdr.width > max_width)
			return ERROR("Bitmap is too wide (% pixels): max width is %", hdr.width, max_width);

		EXPECT(inRange(hdr.width, 0, max_width + 1));
		EXPECT(inRange(hdr.height, 0, max_height + 1));

		sr.seek(sr.pos() + hdr.id_length);

		unsigned bpp = hdr.bits_per_pixel / 8;
		PodVector<IColor> data(hdr.width * hdr.height);
		int2 size(hdr.width, hdr.height);

		bool v_flip = hdr.image_descriptor & 16;
		bool h_flip = hdr.image_descriptor & 32;
		EXPECT(!v_flip && !h_flip && "v_flip & h_flip are not supported");

		if(bpp == 3) {
			for(int y = hdr.height - 1; y >= 0; y--) {
				unsigned char line[max_width * 3];
				sr.loadData(span(line, hdr.width * 3));
				IColor *dst = &data[y * hdr.width];
				for(int x = 0; x < hdr.width; x++)
					dst[x] = IColor(line[x * 3 + 0], line[x * 3 + 1], line[x * 3 + 2]);
			}
		} else if(bpp == 4) {
			for(int y = hdr.height - 1; y >= 0; y--) {
				IColor *colors = &data[y * hdr.width];
				sr.loadData(span(colors, hdr.width));
				for(int x = 0; x < hdr.width; x++)
					colors[x] = colors[x].bgra();
			}
		}
		EXPECT_NO_ERRORS();

		return Texture(move(data), size);
	}
}

Expected<void> Texture::saveTGA(FileStream &sr) const {
	detail::TGAHeader header;

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
		sr.saveData(cspan(&line[0], m_size.x));
	}

	EXPECT_NO_ERRORS();
	return {};
}

Expected<void> Texture::saveTGA(ZStr file_name) const {
	auto file = fileSaver(file_name);
	return file ? saveTGA(*file) : file.error();
}
}
