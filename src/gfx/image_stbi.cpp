// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/image.h"

#include "fwk/io/stream.h"
#include "fwk/sys/expected.h"

#define STBI_NO_PSD
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION

#include "../extern/stb_image.h"

namespace fwk {
namespace detail {
	Ex<Image> loadSTBI(Stream &sr) {
		stbi_io_callbacks callbacks;
		callbacks.read = [](void *user, char *data, int size) -> int {
			auto &stream = *reinterpret_cast<Stream *>(user);
			size = std::min<i64>(stream.size() - stream.pos(), size);
			stream.loadData({data, size});
			return stream.isValid() ? size : 0;
		};
		callbacks.skip = [](void *user, int n) {
			auto &stream = *reinterpret_cast<Stream *>(user);
			stream.seek(stream.pos() + n);
		};
		callbacks.eof = [](void *user) -> int {
			return reinterpret_cast<Stream *>(user)->atEnd() ? 1 : 0;
		};

		int w = 0, h = 0, channels = 0;
		auto *data = stbi_load_from_callbacks(&callbacks, &sr, &w, &h, &channels, 4);
		if(!data) {
			auto error = format("stbi_load_from_callbacks failed: %s", stbi__g_failure_reason);
			sr.reportError(error);
			return sr.getValid().error();
		}

		Image out({w, h}, no_init, VColorFormat::rgba8_unorm);
		// TODO: avoid copy
		copy(out.data(), span(data, w * h * sizeof(IColor)));
		stbi_image_free(data);
		EXPECT(sr.getValid());
		return out;
	}
}
}
