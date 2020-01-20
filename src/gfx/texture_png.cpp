// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/texture.h"

#include <png.h>

#include "fwk/io/file_stream.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/memory.h"

namespace fwk {
namespace detail {

#define FAIL_RET(...)                                                                              \
	{                                                                                              \
		fail(__VA_ARGS__);                                                                         \
		return;                                                                                    \
	}

	struct EXCEPT PngLoader {
		PngLoader(FileStream &stream) : stream(stream) {
			s_loader = this;

			enum { sig_size = 8 };
			unsigned char signature[sig_size];
			stream.loadData(signature);
			stream.seek(stream.pos() - sig_size);
			if(png_sig_cmp(signature, 0, sig_size))
				FAIL_RET("Wrong file signature");

			m_struct = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, 0, errorCallback,
												warningCallback, nullptr, mallocFunc, freeFunc);
			m_info = png_create_info_struct(m_struct);

			if(!m_struct || !m_info)
				FAIL_RET("Error while initializing libpng");

			if(setjmp(png_jmpbuf(m_struct)))
				FAIL_RET();

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

			if(!m_error.empty())
				FAIL_RET();
		}

		~PngLoader() {
			cleanup();
			s_loader = nullptr;
		}

		PngLoader(const PngLoader &) = delete;
		void operator=(const PngLoader &) = delete;

		int2 size() const { return {m_width, m_height}; }

		void operator>>(vector<u16> &out) {
			ASSERT(m_bit_depth == 16);
			ASSERT(m_color_type == PNG_COLOR_TYPE_GRAY);

			vector<u8 *> row_pointers(m_height);

			if(setjmp(png_jmpbuf(m_struct)))
				FAIL_RET();

				// TODO: why swap on mingw?
#if defined(__LITTLE_ENDIAN__) || defined(FWK_PLATFORM_MINGW)
			if(m_bit_depth == 16)
				png_set_swap(m_struct);
#endif

			out.resize(m_width * m_height);
			for(int y = 0; y < m_height; y++)
				row_pointers[y] = (u8 *)&out[m_width * y];
			png_read_image(m_struct, &row_pointers[0]);

			if(!m_error.empty())
				FAIL_RET();
		}

		void operator>>(Span<IColor> out) {
			DASSERT(out.size() >= m_width * m_height);
			vector<u8> temp;
			temp.resize(m_width * m_height * 4);
			vector<u8 *> rowPointers(m_height);

			png_bytep trans = 0;
			int numTrans = 0;

			png_colorp palette = 0;
			int numPalette = 0;

			if(setjmp(png_jmpbuf(m_struct)))
				FAIL_RET();

			for(int y = 0; y < m_height; y++)
				rowPointers[y] = &temp[m_width * 4 * y];

			if(m_color_type == PNG_COLOR_TYPE_PALETTE) {
				if(m_channels != 1)
					FAIL_RET("Only 8-bit palettes are supported");

				if(!png_get_PLTE(m_struct, m_info, &palette, &numPalette))
					FAIL_RET("Error while retrieving palette");

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

			if(!m_error.empty())
				FAIL_RET();
		}

#undef FAIL_RET

	  private:
		void fail(const char *text = "") {
			cleanup();

			string msg = text;
			if(!m_error.empty())
				msg += "\n" + toString(m_error);
			RAISE("PNG loading error: %", msg);
		}

		void cleanup() {
			if(m_struct) {
				png_destroy_read_struct(&m_struct, m_info ? &m_info : nullptr, 0);
				m_struct = nullptr;
				m_info = nullptr;
			}
		}
		static void readCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
			auto *loader = static_cast<PngLoader *>(png_get_io_ptr(png_ptr));
			loader->stream.loadData(span((char *)data, length));
		}

		static void *mallocFunc(png_struct *, size_t count) { return fwk::allocate(count); }
		static void freeFunc(png_struct *, void *ptr) { return fwk::deallocate(ptr); }

		static void errorCallback(png_structp png_ptr, png_const_charp message) {
			if(auto *loader = s_loader)
				loader->m_error = Error(message);
		}
		static void warningCallback(png_structp png_ptr, png_const_charp message) {}

		static __thread PngLoader *s_loader;

		Error m_error;
		FileStream &stream;
		png_structp m_struct = nullptr;
		png_infop m_info = nullptr;
		int m_color_type, m_bit_depth, m_channels;
		int m_width = 0, m_height = 0;
	};

	__thread PngLoader *PngLoader::s_loader = nullptr;

	Ex<Texture> loadPNG(FileStream &stream) {
		PngLoader loader(stream);
		EX_CATCH();
		PodVector<IColor> data(loader.size().x * loader.size().y);
		loader >> data;
		return Texture{move(data), loader.size()};
	}
}

Ex<HeightMap16bit> HeightMap16bit::load(FileStream &stream) {
	detail::PngLoader loader(stream);
	EX_CATCH();
	vector<u16> data(loader.size().x * loader.size().y);
	loader >> data;
	return HeightMap16bit{move(data), loader.size()};
}
}
