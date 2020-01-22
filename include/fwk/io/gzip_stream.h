// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/pod_vector.h"
#include "fwk/sys/error.h"

namespace fwk {

// Simple gzip stream; It does not buffer input data, so it's best
// to save/load data in big blocks (at least few KB).
class GzipStream {
  public:
	GzipStream(GzipStream &&);
	GzipStream &operator=(GzipStream &&);
	~GzipStream();

	GzipStream(const GzipStream &) = delete;
	void operator=(const GzipStream &) = delete;

	// Referenced stream has to exist as long as GzipStream
	static Ex<GzipStream> decompressor(Stream &input, Maybe<i64> load_limit = none);
	static Ex<GzipStream> compressor(Stream &output, int compr_level = 9);

	Ex<int> decompress(Span<char>);
	Ex<vector<char>> decompress();

	Ex<void> compress(CSpan<char>);
	// Don't forget to finish before closing saving stream
	Ex<void> finishCompression();

	bool isFinished() const { return m_is_finished; }

  private:
	GzipStream(void *, Stream &, bool);
	Error makeError(const char *file, int line, Str, int = 0) NOINLINE;

	vector<char> m_buffer;
	Stream *m_pipe = nullptr;
	void *m_ctx = nullptr;
	i64 m_load_limit = -1;
	bool m_is_compressing = false, m_is_valid = true, m_is_finished = false;
};

Ex<vector<char>> gzipCompress(CSpan<char>, int level = 6);
Ex<vector<char>> gzipDecompress(CSpan<char>);
}
