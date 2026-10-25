// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <openexr.h>

#include "fwk/gfx/image.h"

#include "fwk/algorithm.h"
#include "fwk/io/stream.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"

namespace fwk {

namespace {
	Str exrStr(const exr_attr_string_t &str) { return Str(str.str, str.length); }

	struct EXRStreamData {
		Stream &stream;
		i64 offset;
		string error;
	};

	void exrErrorHandler(exr_const_context_t ctx, exr_result_t code, const char *msg) {
		void *user_data = nullptr;
		if(exr_get_user_data(ctx, &user_data) != EXR_ERR_SUCCESS || !user_data)
			return;
		auto &data = *static_cast<EXRStreamData *>(user_data);
		// Only first error is kept, as it's usually the most relevant one
		if(data.error.empty())
			data.error = msg ? msg : exr_get_default_error_message(code);
	}

	int64_t exrReadFunc(exr_const_context_t ctx, void *user_data, void *buffer, uint64_t size,
						uint64_t offset, exr_stream_error_func_ptr_t error_cb) {
		auto &data = *static_cast<EXRStreamData *>(user_data);
		auto &sr = data.stream;
		i64 available = sr.size() - data.offset;
		if(offset >= uint64_t(available))
			return 0;
		i64 count = i64(min(size, uint64_t(available) - offset));

		sr.seek(data.offset + i64(offset));
		if(sr.isValid())
			sr.loadData(Span<char>(static_cast<char *>(buffer), int(count)));
		if(!sr.isValid()) {
			error_cb(ctx, EXR_ERR_READ_IO, "Error while reading from stream");
			return -1;
		}
		return count;
	}

	int64_t exrQuerySizeFunc(exr_const_context_t, void *user_data) {
		auto &data = *static_cast<EXRStreamData *>(user_data);
		return data.stream.size() - data.offset;
	}

	exr_context_initializer_t exrInitializer(EXRStreamData &data) {
		exr_context_initializer_t init = EXR_DEFAULT_CONTEXT_INITIALIZER;
		init.user_data = &data;
		init.error_handler_fn = exrErrorHandler;
		init.read_fn = exrReadFunc;
		init.size_fn = exrQuerySizeFunc;
		return init;
	}

	Error exrError(EXRStreamData &data, exr_result_t code) {
		if(!data.stream.isValid())
			return data.stream.getValid().error();
		string message = data.error.empty() ? exr_get_default_error_message(code) : data.error;
		return FWK_ERROR("Error while loading EXR image: %", message);
	}
}

#define EXR_CHECK(expr)                                                                            \
	{                                                                                              \
		exr_result_t _exr_result = (expr);                                                         \
		if(_exr_result != EXR_ERR_SUCCESS)                                                         \
			return exrError(data, _exr_result);                                                    \
	}

namespace detail {
	Ex<Image> loadEXR(Stream &sr) {
		EXRStreamData data{sr, sr.pos(), {}};
		auto init = exrInitializer(data);
		exr_context_t ctx = nullptr;
		Cleanup ctx_cleanup([&] {
			if(ctx)
				exr_finish(&ctx);
		});
		EXR_CHECK(exr_start_read(&ctx, "stream", &init));

		// Only the first part is loaded
		const int part = 0;
		exr_storage_t storage;
		EXR_CHECK(exr_get_storage(ctx, part, &storage));
		if(storage != EXR_STORAGE_SCANLINE && storage != EXR_STORAGE_TILED)
			return FWK_ERROR("Unsupported EXR storage type (deep images are not supported)");

		exr_attr_box2i_t data_window;
		EXR_CHECK(exr_get_data_window(ctx, part, &data_window));
		i64 width = i64(data_window.max.x) - data_window.min.x + 1;
		i64 height = i64(data_window.max.y) - data_window.min.y + 1;
		const i64 max_size = 8192 * 8192;
		EXPECT(width > 0 && height > 0);
		EXPECT(width * height <= max_size);
		int2 size{int(width), int(height)};

		const exr_attr_chlist_t *channel_list = nullptr;
		EXR_CHECK(exr_get_channels(ctx, part, &channel_list));
		CSpan<exr_attr_chlist_entry_t> channels(channel_list->entries, channel_list->num_channels);
		auto hasChannel = [&](Str name) {
			return anyOf(channels, [&](auto &channel) { return exrStr(channel.name) == name; });
		};

		// Grayscale images (with only Y channel) are expanded to RGB
		bool has_rgb = hasChannel("R") || hasChannel("G") || hasChannel("B");
		bool is_luminance = !has_rgb && hasChannel("Y");
		Str component_names[4] = {"R", "G", "B", "A"};
		if(is_luminance) {
			component_names[0] = "Y";
			component_names[1] = component_names[2] = {};
		}
		auto componentIndex = [&](Str name) {
			return name.empty() ? -1 : indexOf(component_names, name);
		};

		for(auto &channel : channels) {
			if(componentIndex(exrStr(channel.name)) != -1 &&
			   (channel.x_sampling != 1 || channel.y_sampling != 1))
				return FWK_ERROR("Subsampled EXR channels are not supported (channel: %)",
								 exrStr(channel.name));
		}

		// Channels missing in the file are filled with: (0, 0, 0, 1)
		Image out(size, float4(0, 0, 0, 1), VColorFormat::rgba32_sfloat);
		auto *base = out.data().data();
		i64 x_stride = sizeof(float4), y_stride = x_stride * size.x;
		EXPECT(y_stride <= INT_MAX);

		exr_decode_pipeline_t pipeline = EXR_DECODE_PIPELINE_INITIALIZER;
		bool pipeline_initialized = false;
		Cleanup pipeline_cleanup([&] {
			if(pipeline_initialized)
				exr_decoding_destroy(ctx, &pipeline);
		});
		// Position is relative to the data window
		auto decodeChunk = [&](const exr_chunk_info_t &chunk, int x, int y) -> exr_result_t {
			if(x < 0 || y < 0 || chunk.width < 0 || chunk.height < 0 ||
			   i64(x) + chunk.width > size.x || i64(y) + chunk.height > size.y)
				return EXR_ERR_CORRUPT_CHUNK;

			exr_result_t result;
			if(pipeline_initialized) {
				result = exr_decoding_update(ctx, part, &chunk, &pipeline);
			} else {
				result = exr_decoding_initialize(ctx, part, &chunk, &pipeline);
				pipeline_initialized = result == EXR_ERR_SUCCESS;
			}
			if(result != EXR_ERR_SUCCESS)
				return result;

			for(int i = 0; i < pipeline.channel_count; i++) {
				auto &channel = pipeline.channels[i];
				int c = componentIndex(Str(channel.channel_name));
				if(c == -1) {
					channel.decode_to_ptr = nullptr;
					continue;
				}
				channel.decode_to_ptr = base + y * y_stride + x * x_stride + c * i64(sizeof(float));
				channel.user_pixel_stride = int32_t(x_stride);
				channel.user_line_stride = int32_t(y_stride);
				channel.user_data_type = EXR_PIXEL_FLOAT;
				channel.user_bytes_per_element = int16_t(sizeof(float));
			}

			result = exr_decoding_choose_default_routines(ctx, part, &pipeline);
			if(result != EXR_ERR_SUCCESS)
				return result;
			return exr_decoding_run(ctx, part, &pipeline);
		};

		if(storage == EXR_STORAGE_SCANLINE) {
			int32_t lines_per_chunk = 0;
			EXR_CHECK(exr_get_scanlines_per_chunk(ctx, part, &lines_per_chunk));
			EXPECT(lines_per_chunk > 0);
			for(i64 y = data_window.min.y; y <= data_window.max.y; y += lines_per_chunk) {
				exr_chunk_info_t chunk;
				EXR_CHECK(exr_read_scanline_chunk_info(ctx, part, int(y), &chunk));
				int chunk_y = int(i64(chunk.start_y) - data_window.min.y);
				EXR_CHECK(decodeChunk(chunk, 0, chunk_y));
			}
		} else {
			// Only the first (full resolution) level is loaded
			int32_t tile_width = 0, tile_height = 0, num_tiles_x = 0, num_tiles_y = 0;
			EXR_CHECK(exr_get_tile_sizes(ctx, part, 0, 0, &tile_width, &tile_height));
			EXR_CHECK(exr_get_tile_counts(ctx, part, 0, 0, &num_tiles_x, &num_tiles_y));
			EXPECT(tile_width > 0 && tile_height > 0);
			for(int ty = 0; ty < num_tiles_y; ty++)
				for(int tx = 0; tx < num_tiles_x; tx++) {
					exr_chunk_info_t chunk;
					EXR_CHECK(exr_read_tile_chunk_info(ctx, part, tx, ty, 0, 0, &chunk));
					i64 x = i64(tx) * tile_width, y = i64(ty) * tile_height;
					if(x >= size.x || y >= size.y)
						return FWK_ERROR("Invalid EXR tile position");
					EXR_CHECK(decodeChunk(chunk, int(x), int(y)));
				}
		}

		if(is_luminance)
			for(int y = 0; y < size.y; y++)
				for(auto &pixel : out.row<float4>(y))
					pixel.y = pixel.z = pixel.x;
		return out;
	}
}

#undef EXR_CHECK
}
