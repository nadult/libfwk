// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/pod_vector.h"
#include "fwk/sys/error.h"

namespace fwk {

// Simple zstd stream; It does not buffer input data, so it's best
// to save/load data in big blocks (at least few KB).
class ZstdStream {
  public:
	ZstdStream(ZstdStream &&);
	ZstdStream &operator=(ZstdStream &&);
	~ZstdStream();

	ZstdStream(const ZstdStream &) = delete;
	void operator=(const ZstdStream &) = delete;

	static constexpr int default_comp_level = 3;
	static Pair<int> compressionLevelRange();

	// Referenced stream has to exist as long as ZstdStream
	static Ex<ZstdStream> decompressor(Stream &input, Maybe<i64> load_limit = none);
	static Ex<ZstdStream> compressor(Stream &output, int compr_level = default_comp_level);

	Ex<int> decompress(Span<char>);
	Ex<vector<char>> decompress();

	Ex<> compress(CSpan<char>);
	// Don't forget to finish before closing saving stream
	Ex<> finishCompression();

	bool isFinished() const { return m_is_finished; }

  private:
	ZstdStream(void *, Stream &, bool);
	FWK_NO_INLINE Error makeError(const char *file, int line, Str);
	Ex<> flushOutput();

	vector<char> m_buffer;
	Stream *m_pipe = nullptr;
	void *m_ctx = nullptr;
	i64 m_load_limit = -1;
	// Decompression: range of valid input data in m_buffer
	// Compression: m_buffer_pos is the amount of compressed data waiting in m_buffer
	int m_buffer_pos = 0, m_buffer_size = 0;
	bool m_is_compressing = false, m_is_valid = true, m_is_finished = false;
};

Ex<vector<char>> zstdCompress(CSpan<char>, int level = ZstdStream::default_comp_level);
Ex<vector<char>> zstdDecompress(CSpan<char>);
}
