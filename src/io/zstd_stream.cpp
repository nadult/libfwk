// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/zstd_stream.h"

#include "fwk/io/memory_stream.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"
#include <zstd.h>

namespace fwk {

using namespace std;

static constexpr int buffer_size = 64 * 1024;

ZstdStream::ZstdStream(void *ctx, Stream &pipe, bool is_compressing)
	: m_buffer(buffer_size), m_pipe(&pipe), m_ctx(ctx), m_is_compressing(is_compressing) {}

ZstdStream::ZstdStream(ZstdStream &&rhs)
	: m_buffer(std::move(rhs.m_buffer)), m_pipe(rhs.m_pipe), m_ctx(rhs.m_ctx),
	  m_load_limit(rhs.m_load_limit), m_buffer_pos(rhs.m_buffer_pos),
	  m_buffer_size(rhs.m_buffer_size), m_is_compressing(rhs.m_is_compressing),
	  m_is_valid(rhs.m_is_valid), m_is_finished(rhs.m_is_finished) {
	rhs.m_pipe = nullptr;
	rhs.m_ctx = nullptr;
	rhs.m_is_valid = false;
	rhs.m_is_finished = true;
}

FWK_MOVE_ASSIGN_RECONSTRUCT(ZstdStream);

ZstdStream::~ZstdStream() {
	if(m_ctx) {
		if(m_is_compressing) {
			ZSTD_freeCCtx((ZSTD_CCtx *)m_ctx);
			DASSERT(m_is_finished);
		} else {
			ZSTD_freeDCtx((ZSTD_DCtx *)m_ctx);
		}
	}
}

Pair<int> ZstdStream::compressionLevelRange() { return {ZSTD_minCLevel(), ZSTD_maxCLevel()}; }

// TODO: use FWK_FATAL in these?
Ex<ZstdStream> ZstdStream::decompressor(Stream &input, Maybe<i64> load_limit) {
	DASSERT(input.isLoading());

	auto ctx = ZSTD_createDCtx();
	if(!ctx)
		return FWK_ERROR("ZSTD_createDCtx failed");
	ZstdStream out(ctx, input, false);
	if(load_limit) {
		DASSERT(*load_limit >= 0);
		out.m_load_limit = *load_limit;
	}
	return out;
}

Ex<ZstdStream> ZstdStream::compressor(Stream &output, int compr_level) {
	DASSERT(output.isSaving());
	DASSERT(compr_level >= ZSTD_minCLevel() && compr_level <= ZSTD_maxCLevel());

	auto ctx = ZSTD_createCCtx();
	if(!ctx)
		return FWK_ERROR("ZSTD_createCCtx failed");
	auto ret = ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, compr_level);
	if(ZSTD_isError(ret)) {
		ZSTD_freeCCtx(ctx);
		const char *err_name = ZSTD_getErrorName(ret);
		return FWK_ERROR("Error while setting compression level to %: %", compr_level, err_name);
	}
	return ZstdStream(ctx, output, true);
}

Error ZstdStream::makeError(const char *file, int line, Str str) {
	m_is_valid = false;
	auto text = format("Error while % (stream pos:%): %",
					   m_is_compressing ? "compressing" : "decompressing", m_pipe->pos(), str);
	auto out = Error(ErrorLoc{file, line}, std::move(text));

	if(auto result = m_pipe->getValid(); !result)
		out = Error::merge({std::move(out), std::move(result.error())});
	return out;
}

#define ZSTD_ERROR(...) makeError(__FILE__, __LINE__, __VA_ARGS__)

Ex<int> ZstdStream::decompress(Span<char> data) {
	PASSERT(!m_is_compressing);
	if(!m_is_valid)
		return ZSTD_ERROR("Reading from invalidated stream");
	if(m_is_finished || !data)
		return 0;

	auto ctx = (ZSTD_DCtx *)m_ctx;
	size_t out_pos = 0;

	while(out_pos < size_t(data.size())) {
		if(m_buffer_pos == m_buffer_size) {
			i64 max_read = min(m_buffer.size(), (int)(m_pipe->size() - m_pipe->pos()));
			if(m_load_limit != -1) {
				max_read = min(max_read, m_load_limit);
				m_load_limit -= max_read;
			}

			m_pipe->loadData(span(m_buffer.data(), max_read));
			if(!m_pipe->isValid())
				return ZSTD_ERROR("Stream error during decompression");

			m_buffer_pos = 0;
			m_buffer_size = int(max_read);
		}

		ZSTD_inBuffer in{m_buffer.data(), size_t(m_buffer_size), size_t(m_buffer_pos)};
		ZSTD_outBuffer out{data.data(), size_t(data.size()), out_pos};
		auto ret = ZSTD_decompressStream(ctx, &out, &in);
		if(ZSTD_isError(ret)) {
			const char *err_name = ZSTD_getErrorName(ret);
			return ZSTD_ERROR(format("ZSTD_decompressStream failed: %", err_name));
		}

		bool made_progress = in.pos != size_t(m_buffer_pos) || out.pos != out_pos;
		m_buffer_pos = int(in.pos);
		out_pos = out.pos;

		// 0 means that frame was fully decoded & flushed
		if(ret == 0) {
			m_is_finished = true;
			break;
		}
		if(!made_progress)
			return ZSTD_ERROR("Unexpected end of compressed data");
	}

	return int(out_pos);
}

Ex<vector<char>> ZstdStream::decompress() {
	vector<char> out;
	while(!isFinished()) {
		char buffer[buffer_size];
		if(auto result = decompress(buffer))
			insertBack(out, cspan(buffer, *result));
		else
			return result.error();
	}
	return out;
}

Ex<> ZstdStream::flushOutput() {
	if(m_buffer_pos > 0) {
		m_pipe->saveData(cspan(m_buffer.data(), m_buffer_pos));
		m_buffer_pos = 0;
		if(!m_pipe->isValid())
			return ZSTD_ERROR("Stream error during compression");
	}
	return {};
}

Ex<> ZstdStream::compress(CSpan<char> data) {
	PASSERT(m_is_compressing && !m_is_finished);
	if(!m_is_valid)
		return ZSTD_ERROR("Writing to invalidated stream");
	if(!data)
		return {};

	auto ctx = (ZSTD_CCtx *)m_ctx;
	ZSTD_inBuffer in{data.data(), size_t(data.size()), 0};

	while(in.pos < in.size) {
		ZSTD_outBuffer out{m_buffer.data(), size_t(m_buffer.size()), size_t(m_buffer_pos)};
		auto ret = ZSTD_compressStream2(ctx, &out, &in, ZSTD_e_continue);
		if(ZSTD_isError(ret))
			return ZSTD_ERROR(format("ZSTD_compressStream2 failed: %", ZSTD_getErrorName(ret)));
		m_buffer_pos = int(out.pos);

		if(m_buffer_pos == m_buffer.size())
			EXPECT(flushOutput());
	}

	return {};
}

Ex<> ZstdStream::finishCompression() {
	PASSERT(m_is_compressing && !m_is_finished);
	if(!m_is_valid)
		return FWK_ERROR("Writing to invalidated stream");

	auto ctx = (ZSTD_CCtx *)m_ctx;
	ZSTD_inBuffer in{nullptr, 0, 0};

	size_t remaining = 0;
	do {
		ZSTD_outBuffer out{m_buffer.data(), size_t(m_buffer.size()), size_t(m_buffer_pos)};
		remaining = ZSTD_compressStream2(ctx, &out, &in, ZSTD_e_end);
		if(ZSTD_isError(remaining))
			return ZSTD_ERROR(
				format("ZSTD_compressStream2 failed: %", ZSTD_getErrorName(remaining)));
		m_buffer_pos = int(out.pos);
		EXPECT(flushOutput());
	} while(remaining != 0);
	m_is_finished = true;

	return {};
}

Ex<vector<char>> zstdCompress(CSpan<char> data, int level) {
	auto output = memorySaver(data.size());
	auto stream = EX_PASS(ZstdStream::compressor(output, level));
	EXPECT(stream.compress(data));
	EXPECT(stream.finishCompression());
	int data_size = output.size();
	vector<char> out;
	auto buffer = output.extractBuffer();
	buffer.resize(data_size);
	buffer.unsafeSwap(out);
	return out;
}

Ex<vector<char>> zstdDecompress(CSpan<char> data) {
	auto input = memoryLoader(data);
	auto stream = EX_PASS(ZstdStream::decompressor(input));
	return stream.decompress();
}
}

#undef ZSTD_ERROR