// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/image.h"

#include "fwk/io/file_stream.h"
#include "fwk/io/stream.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_internal.h"

// KTX2 specification: https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html
// Not supported: supercompression, 3D images, image arrays & cube maps.

namespace fwk {

namespace {
	struct KTX2Header {
		void load(Stream &sr) {
			sr.unpack(vk_format, type_size, pixel_width, pixel_height, pixel_depth, layer_count,
					  face_count, level_count, supercompression_scheme);
		}

		u32 vk_format, type_size;
		u32 pixel_width, pixel_height, pixel_depth;
		u32 layer_count, face_count, level_count;
		u32 supercompression_scheme;
	};

	struct KTX2Level {
		void load(Stream &sr) { sr.unpack(byte_offset, byte_length, uncompressed_byte_length); }

		u64 byte_offset, byte_length, uncompressed_byte_length;
	};

	// Loads at most max_levels mipmap levels, starting from the largest one
	Ex<vector<Image>> loadKTX2Levels(Stream &sr, int max_levels) {
		// All offsets in KTX2 are relative to the beginning of the file
		i64 file_offset = sr.pos();
		EXPECT(sr.loadSignature("\xABKTX 20\xBB\r\n\x1A\n"));

		KTX2Header header;
		header.load(sr);
		// Skipping index of data format descriptor, key/value data & supercompression global data
		char index[32];
		sr.loadData(Span<char>(index, sizeof(index)));
		EXPECT(sr.getValid());

		if(header.supercompression_scheme != 0)
			return FWK_ERROR("KTX2 supercompression is not supported (scheme: %)",
							 header.supercompression_scheme);
		if(header.vk_format == 0)
			return FWK_ERROR(
				"KTX2 images with VK_FORMAT_UNDEFINED (e.g. Basis Universal) are not supported");
		auto format = fromVkColorFormat(VkFormat(header.vk_format));
		if(!format)
			return FWK_ERROR("Unsupported KTX2 image format: VkFormat(%)", header.vk_format);
		if(header.pixel_depth != 0)
			return FWK_ERROR("3D KTX2 images are not supported");
		if(header.layer_count > 1)
			return FWK_ERROR("KTX2 image arrays are not supported");
		if(header.face_count == 6)
			return FWK_ERROR("KTX2 cube maps are not supported");
		EXPECT(header.face_count == 1);

		// pixel_height == 0 means that it's a 1D image
		const u32 max_size = 65536, max_pixels = 8192 * 8192;
		EXPECT(header.pixel_width >= 1 && header.pixel_width <= max_size);
		EXPECT(header.pixel_height <= max_size);
		EXPECT(u64(header.pixel_width) * u64(header.pixel_height) <= max_pixels);
		int2 size(header.pixel_width, max(int(header.pixel_height), 1));

		int level_count = max(int(header.level_count), 1);
		EXPECT(level_count <= Image::maxMipmapLevels(size));
		vector<KTX2Level> levels(level_count);
		for(auto &level : levels)
			level.load(sr);
		EXPECT(sr.getValid());

		u64 file_size = u64(sr.size() - file_offset);
		vector<Image> out;
		int num_levels = min(levels.size(), max_levels);
		for(int i = 0; i < num_levels; i++) {
			auto &level = levels[i];
			int2 level_size(max(size.x >> i, 1), max(size.y >> i, 1));
			int2 block_size = imageBlockSize(*format, level_size);
			u64 data_size = u64(block_size.x) * block_size.y * unitByteSize(*format);

			if(level.byte_length != data_size)
				return FWK_ERROR("Invalid KTX2 data size for mip level %: % (expected: %)", i,
								 level.byte_length, data_size);
			if(level.byte_offset > file_size || data_size > file_size - level.byte_offset)
				return FWK_ERROR("KTX2 data for mip level % is out of file bounds", i);

			PodVector<u8> data(data_size);
			sr.seek(file_offset + i64(level.byte_offset));
			sr.loadData(Span<u8>(data.data(), data.size()));
			EXPECT(sr.getValid());
			out.emplace_back(std::move(data), level_size, *format);
		}

		return out;
	}
}

namespace detail {
	Ex<Image> loadKTX2(Stream &sr) {
		auto levels = EX_PASS(loadKTX2Levels(sr, 1));
		return std::move(levels[0]);
	}
}

Ex<vector<Image>> Image::loadKTX2(Stream &sr) { return loadKTX2Levels(sr, INT_MAX); }

Ex<vector<Image>> Image::loadKTX2(ZStr file_name) {
	auto loader = EX_PASS(fileLoader(file_name));
	return loadKTX2(loader);
}
}
