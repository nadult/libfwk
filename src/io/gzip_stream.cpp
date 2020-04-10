// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/gzip_stream.h"

#include "fwk/io/memory_stream.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"
#include "stream_inl.h"
#include <zlib.h>

namespace fwk {

using namespace std;

static constexpr int buffer_size = 16 * 1024;

// TODO: slow compression (5MB/s)

GzipStream::GzipStream(void *ctx, Stream &pipe, bool is_compressing)
	: m_buffer(buffer_size), m_pipe(&pipe), m_ctx(ctx), m_is_compressing(is_compressing) {}

GzipStream::GzipStream(GzipStream &&rhs)
	: m_buffer(move(rhs.m_buffer)), m_pipe(rhs.m_pipe), m_ctx(rhs.m_ctx),
	  m_is_compressing(rhs.m_is_compressing), m_is_valid(rhs.m_is_valid),
	  m_is_finished(rhs.m_is_finished) {
	rhs.m_pipe = nullptr;
	rhs.m_ctx = nullptr;
	rhs.m_is_valid = false;
	rhs.m_is_finished = true;
}

FWK_MOVE_ASSIGN_RECONSTRUCT(GzipStream);

GzipStream::~GzipStream() {
	if(m_ctx) {
		if(m_pipe->isLoading())
			inflateEnd((z_stream *)m_ctx);
		else {
			deflateEnd((z_stream *)m_ctx);
			DASSERT(m_is_finished);
		}
		fwk::deallocate(m_ctx);
	}
}

// TODO: use FATAL in these?
Ex<GzipStream> GzipStream::decompressor(Stream &input, Maybe<i64> load_limit) {
	DASSERT(input.isLoading());

	auto ctx = (z_stream *)fwk::allocate(sizeof(z_stream));
	memset(ctx, 0, sizeof(*ctx));

	if(inflateInit2(ctx, 32 + 15) != Z_OK) {
		fwk::deallocate(ctx);
		return ERROR("inflateInit failed");
	}
	GzipStream out(ctx, input, false);
	if(load_limit) {
		DASSERT(*load_limit >= 0);
		out.m_load_limit = *load_limit;
	}
	return out;
}

Ex<GzipStream> GzipStream::compressor(Stream &output, int compr_level) {
	DASSERT(output.isSaving());
	DASSERT(compr_level >= 0 && compr_level <= 9);

	auto ctx = (z_stream *)fwk::allocate(sizeof(z_stream));
	memset(ctx, 0, sizeof(*ctx));

	if(deflateInit(ctx, compr_level) != Z_OK) {
		fwk::deallocate(ctx);
		return ERROR("Error in deflateInit");
	}
	auto out = GzipStream(ctx, output, true);
	ctx->avail_out = out.m_buffer.size();
	ctx->next_out = (Bytef *)out.m_buffer.data();
	return out;
}

Error GzipStream::makeError(const char *file, int line, Str str, int err) {
	m_is_valid = false;
	auto ctx = (z_stream *)m_ctx;
	int input_pos = ctx ? ctx->total_in : 0, output_pos = ctx ? ctx->total_out : 0;
	auto text =
		format("Error while % (input pos:% output_pos:%): %",
			   m_is_compressing ? "compressing" : "decompressing", input_pos, output_pos, str);
	if(err)
		text += format(" err:%", err);
	auto out = Error(ErrorLoc{file, line}, move(text));

	if(exceptionRaised())
		out = Error::merge({out, getMergedExceptions()});
	return out;
}

#define GZERROR(...) makeError(__FILE__, __LINE__, __VA_ARGS__)

Ex<int> GzipStream::decompress(Span<char> data) {
	PASSERT(!m_is_compressing);
	if(!m_is_valid)
		return GZERROR("Reading from invalidated stream");
	if(m_is_finished || !data)
		return 0;

	auto &ctx = *(z_stream *)m_ctx;
	int out_pos = 0;

	while(out_pos < data.size()) {
		if(!ctx.avail_in) {
			i64 max_read = min(m_buffer.size(), (int)(m_pipe->size() - m_pipe->pos()));
			if(m_load_limit != -1) {
				max_read = min(max_read, m_load_limit);
				m_load_limit -= max_read;
			}

			m_pipe->loadData(span(m_buffer.data(), max_read));
			if(exceptionRaised())
				return GZERROR("Exception while reading data from input stream");

			ctx.avail_in = max_read;
			ctx.next_in = (Bytef *)m_buffer.data();
		}

		ctx.avail_out = data.size() - out_pos;
		ctx.next_out = (Bytef *)data.data() + out_pos;
		auto ret = inflate(&ctx, Z_NO_FLUSH);

		if(isOneOf(ret, Z_STREAM_ERROR, Z_MEM_ERROR, Z_DATA_ERROR))
			return GZERROR("inflate failed", ret);
		out_pos = data.size() - ctx.avail_out;

		if(ret == Z_STREAM_END) {
			m_is_finished = true;
			break;
		}
	}

	return out_pos;
}

Ex<vector<char>> GzipStream::decompress() {
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

Ex<void> GzipStream::compress(CSpan<char> data) {
	PASSERT(m_is_compressing && !m_is_finished);
	if(!m_is_valid)
		return ERROR("Writing to invalidated stream");
	if(!data)
		return {};

	auto &ctx = *(z_stream *)m_ctx;
	ctx.avail_in = data.size();
	ctx.next_in = (Bytef *)data.data();
	PASSERT(ctx.avail_out);

	while(ctx.avail_in > 0) {
		auto ret = deflate(&ctx, Z_NO_FLUSH);

		if(isOneOf(ret, Z_STREAM_ERROR, Z_MEM_ERROR, Z_DATA_ERROR))
			return GZERROR("deflate failed", ret);

		// Flusing buffer
		if(ctx.avail_out == 0) {
			m_pipe->saveData(m_buffer);
			if(exceptionRaised())
				return GZERROR("Exception while reading data from input stream");
			ctx.avail_out = m_buffer.size();
			ctx.next_out = (Bytef *)m_buffer.data();
		}
	}

	return {};
}

Ex<void> GzipStream::finishCompression() {
	PASSERT(m_is_compressing && !m_is_finished);
	if(!m_is_valid)
		return ERROR("Writing to invalidated stream");

	auto &ctx = *(z_stream *)m_ctx;
	ctx.avail_in = 0;
	ctx.next_in = nullptr;
	PASSERT(ctx.avail_out);

	int ret = 0;
	do {
		ret = deflate(&ctx, Z_FINISH);

		if(isOneOf(ret, Z_STREAM_ERROR, Z_MEM_ERROR, Z_DATA_ERROR))
			return GZERROR("deflate failed", ret);

		if(ctx.avail_out < (uint)m_buffer.size() || ret == Z_STREAM_END) {
			m_pipe->saveData(cspan(m_buffer.data(), m_buffer.size() - ctx.avail_out));
			if(exceptionRaised())
				return GZERROR("Exception while reading data from input stream");
			ctx.avail_out = m_buffer.size();
			ctx.next_out = (Bytef *)m_buffer.data();
		}
	} while(ret != Z_STREAM_END);
	m_is_finished = true;

	return {};
}

Ex<vector<char>> gzipCompress(CSpan<char> data, int level) {
	auto output = memorySaver(data.size());
	auto stream = EX_PASS(GzipStream::compressor(output, level));
	EXPECT(stream.compress(data));
	EXPECT(stream.finishCompression());
	int data_size = output.size();
	vector<char> out;
	auto buffer = output.extractBuffer();
	buffer.resize(data_size);
	buffer.unsafeSwap(out);
	return out;
}

Ex<vector<char>> gzipDecompress(CSpan<char> data) {
	auto input = memoryLoader(data);
	auto stream = EX_PASS(GzipStream::decompressor(input));
	return stream.decompress();
}

u32 crc32(CSpan<u8> data) { return ::crc32(0, data.data(), data.size()); }
}
