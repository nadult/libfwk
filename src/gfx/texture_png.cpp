// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/texture.h"
#include <png.h>

namespace fwk {
namespace detail {

	class PngLoader {
	  public:
		PngLoader(Stream &stream) : stream(stream), m_struct(nullptr), m_info(nullptr) {
			enum { sig_size = 8 };
			unsigned char signature[sig_size];
			stream.loadData(signature, sig_size);
			stream.seek(stream.pos() - sig_size);
			if(png_sig_cmp(signature, 0, sig_size))
				THROW("Wrong PNG file signature");

			m_struct =
				png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, errorCallback, warningCallback);
			m_info = png_create_info_struct(m_struct);

			if(!m_struct || !m_info) {
				finalCleanup();
				THROW("Error while initializing PNGLoader");
			}

			png_set_read_fn(m_struct, this, readCallback);
			png_read_info(m_struct, m_info);

			png_uint_32 width, height;

			png_get_IHDR(m_struct, m_info, &width, &height, &m_bit_depth, &m_color_type, 0, 0, 0);

			if(m_color_type == PNG_COLOR_TYPE_GRAY && m_bit_depth < 8)
				png_set_expand_gray_1_2_4_to_8(m_struct);

			if(png_get_valid(m_struct, m_info, PNG_INFO_tRNS) &&
			   !(png_get_valid(m_struct, m_info, PNG_INFO_PLTE)))
				png_set_tRNS_to_alpha(m_struct);

			png_get_IHDR(m_struct, m_info, &width, &height, &m_bit_depth, &m_color_type, 0, 0, 0);

			if(m_bit_depth < 8) {
				m_bit_depth = 8;
				png_set_packing(m_struct);
			}

			png_read_update_info(m_struct, m_info);
			m_channels = (int)png_get_channels(m_struct, m_info);
			m_color_type = png_get_color_type(m_struct, m_info);

			m_width = width;
			m_height = height;
		}

		~PngLoader() { finalCleanup(); }
		PngLoader(const PngLoader &) = delete;
		void operator=(const PngLoader &) = delete;

		int2 size() const { return {m_width, m_height}; }

		void operator>>(vector<u16> &out) const {
			ASSERT(m_bit_depth == 16);
			ASSERT(m_color_type == PNG_COLOR_TYPE_GRAY);

#if defined(__LITTLE_ENDIAN__) || defined(FWK_TARGET_MINGW)
			if(m_bit_depth == 16)
				png_set_swap(m_struct);
#endif

			vector<u8 *> row_pointers(m_height);
			out.resize(m_width * m_height);
			for(int y = 0; y < m_height; y++)
				row_pointers[y] = (u8 *)&out[m_width * y];
			png_read_image(m_struct, &row_pointers[0]);
		}

		void operator>>(Span<IColor> out) const {
			DASSERT(out.size() >= m_width * m_height);
			vector<u8> temp;
			temp.resize(m_width * m_height * 4);
			vector<u8 *> rowPointers(m_height);

			png_bytep trans = 0;
			int numTrans = 0;

			png_colorp palette = 0;
			int numPalette = 0;

			for(int y = 0; y < m_height; y++)
				rowPointers[y] = &temp[m_width * 4 * y];

			if(m_color_type == PNG_COLOR_TYPE_PALETTE) {
				if(m_channels != 1)
					THROW("Only 8-bit palettes are supported");

				if(!png_get_PLTE(m_struct, m_info, &palette, &numPalette))
					THROW("Error while retrieving PNG palette");

				if(png_get_valid(m_struct, m_info, PNG_INFO_tRNS))
					png_get_tRNS(m_struct, m_info, &trans, &numTrans, 0);
			}

			png_read_image(m_struct, &rowPointers[0]);

			for(int y = 0; y < m_height; y++) {
				IColor *dst = out.data() + m_width * y;
				u8 *src = rowPointers[y];

				if(m_color_type == PNG_COLOR_TYPE_PALETTE) {
					for(int x = 0; x < m_width; x++) {
						png_color &col = palette[src[x]];
						dst[x] = IColor(col.red, col.green, col.blue, trans ? trans[src[x]] : 255u);
					}
				} else if(m_color_type == PNG_COLOR_TYPE_GRAY) {
					for(int x = 0; x < m_width; x++)
						dst[x] = IColor(src[x], src[x], src[x]);
				} else if(m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
					for(int x = 0; x < m_width; x++)
						dst[x] = IColor(src[x * 2], src[x * 2], src[x * 2], src[x * 2 + 1]);
				} else if(m_color_type == PNG_COLOR_TYPE_RGB) {
					for(int x = 0; x < m_width; x++)
						dst[x] = IColor(src[x * 3 + 0], src[x * 3 + 1], src[x * 3 + 2]);
				} else if(m_color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
					for(int x = 0; x < m_width; x++) // TODO: check
						dst[x] =
							IColor(src[x * 4 + 0], src[x * 4 + 1], src[x * 4 + 2], src[x * 4 + 3]);
				}
			}
		}

	  private:
		void finalCleanup() { png_destroy_read_struct(&m_struct, &m_info, 0); }
		static void readCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
			auto *loader = static_cast<PngLoader *>(png_get_io_ptr(png_ptr));
			loader->stream.loadData(data, length);
		}

		static void errorCallback(png_structp png_ptr, png_const_charp message) {
			THROW("LibPNG Error: %s", message);
		}

		static void warningCallback(png_structp png_ptr, png_const_charp message) {}

		Stream &stream;
		png_structp m_struct;
		png_infop m_info;
		int m_color_type, m_bit_depth, m_channels;
		int m_width, m_height;
	};

	void loadPNG(Stream &stream, PodArray<IColor> &out_data, int2 &out_size) {
		PngLoader loader(stream);
		out_size = loader.size();
		out_data.resize(out_size.x * out_size.y);
		loader >> out_data;
	}
}

void HeightMap16bit::load(Stream &stream) {
	detail::PngLoader loader(stream);
	size = loader.size();
	loader >> data;
}
}
