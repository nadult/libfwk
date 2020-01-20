// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/texture.h"

#include "fwk/io/file_stream.h"
#include "fwk/sys/expected.h"

namespace fwk {
namespace detail {
	Ex<Texture> loadBMP(FileStream &sr) {
		sr.signature("BM");

		int offset;
		{
			i32 size, reserved, toffset;
			sr.unpack(size, reserved, toffset);
			offset = toffset;
		}

		int width, height, bpp;
		{
			i32 hSize;
			sr >> hSize;
			if(hSize == 12) {
				u16 hwidth, hheight, planes, hbpp;
				sr.unpack(hwidth, hheight, planes, hbpp);

				width = hwidth;
				height = hheight;
				bpp = hbpp;
			} else {
				i32 hwidth, hheight;
				i16 planes, hbpp;
				i32 compr, temp[5];

				sr.unpack(hwidth, hheight, planes, hbpp);
				sr >> compr;
				sr.loadData(temp);
				width = hwidth;
				height = hheight;
				bpp = hbpp;
				if(hSize > 40)
					sr.seek(sr.pos() + hSize - 40);

				EXPECT(compr == 0 && "Compressed bitmaps not supported");
			}
		}

		EXPECT(width >= 0 && height >= 0);

		int max_size = 4096;
		if(bpp != 24 && bpp != 32 && bpp != 8)
			return ERROR("%-bit bitmaps are not supported (only 8, 24 and 32)", bpp);
		if(width > max_size || height > max_size)
			return ERROR("Bitmap is too big (% x %): max width/height: %", width, height, max_size);

		int bytesPerPixel = bpp / 8;
		int lineAlignment = 4 * ((bpp * width + 31) / 32) - bytesPerPixel * width;

		PodVector<IColor> data(width * height);

		// TODO: when loading from file we should also automatically return info about this file

		u8 line[max_size * 3];

		if(bytesPerPixel == 1) {
			IColor palette[256];
			sr.loadData(palette); // TODO: check if palette is ok
			if(offset > sr.size())
				return ERROR("Invalid data offset: % / %", offset, sr.size());
			sr.seek(offset);

			for(uint n = 0; n < arraySize(palette); n++)
				palette[n].a = 255;

			for(int y = height - 1; y >= 0; y--) {
				IColor *dst = &data[width * y];
				sr.loadData(span(line, width));
				sr.seek(sr.pos() + lineAlignment);

				for(int x = 0; x < width; x++)
					dst[x] = palette[line[x]];
			}
		} else if(bytesPerPixel == 3) {
			sr.seek(offset);
			for(int y = height - 1; y >= 0; y--) {
				sr.loadData(span(line, width * 3));

				IColor *dst = &data[width * y];
				for(int x = 0; x < width; x++)
					dst[x] =
						IColor(line[x * 3 + 0], line[x * 3 + 1], line[x * 3 + 2]); // TODO: check me
				sr.seek(sr.pos() + lineAlignment);
			}
		} else if(bytesPerPixel == 4) {
			sr.seek(offset);
			for(int y = height - 1; y >= 0; y--) {
				sr.loadData(span(&data[width * y], width * 4));
				sr.seek(sr.pos() + lineAlignment);
			}
		}

		return Texture(move(data), {width, height});
	}
}
}
