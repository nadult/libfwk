// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/gzip_stream.h"

#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"
#include "stream_inl.h"
#include <zlib.h>

namespace fwk {

using namespace std;
	  
static constexpr int buffer_size = 16 * 1024;

// TODO: slow compression (5MB/s)

GzipStream::GzipStream(void *ctx, Stream &pipe, bool is_loading)
	: m_buffer(buffer_size), m_pipe(&pipe), m_ctx(ctx), m_is_loading(is_loading) {}

GzipStream::GzipStream(GzipStream &&rhs)
	: m_buffer(move(rhs.m_buffer)), m_pipe(rhs.m_pipe), m_ctx(rhs.m_ctx),
	  m_is_loading(rhs.m_is_loading), m_is_valid(rhs.m_is_valid), m_is_finished(rhs.m_is_finished) {
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

Ex<GzipStream> GzipStream::loader(Stream &input, Maybe<i64> load_limit) {
	DASSERT(input.isLoading());

	auto ctx = (z_stream *)fwk::allocate(sizeof(z_stream));
	memset(ctx, 0, sizeof(*ctx));

	if(inflateInit(ctx) != Z_OK) {
		fwk::deallocate(ctx);
		return ERROR("Error in inflateInit");
	}
	GzipStream out(ctx, input, true);
	if(load_limit) {
		DASSERT(*load_limit >= 0);
		out.m_load_limit = *load_limit;
	}
	return out;
}

Ex<GzipStream> GzipStream::saver(Stream &output, int compr_level) {
	DASSERT(output.isSaving());
	DASSERT(compr_level >= 0 && compr_level <= 9);

	auto ctx = (z_stream *)fwk::allocate(sizeof(z_stream));
	memset(ctx, 0, sizeof(*ctx));

	if(deflateInit(ctx, compr_level) != Z_OK) {
		fwk::deallocate(ctx);
		return ERROR("Error in deflateInit");
	}
	auto out = GzipStream(ctx, output, false);
	ctx->avail_out = out.m_buffer.size();
	ctx->next_out = (Bytef*)out.m_buffer.data();
	return out;
}

/*
void GzipStream::raise(ZStr text) {
	RAISE("GzipStream %s error at position %lld/%lld: %s",
		  isLoading() ? "loading(inflating)" : "saving(deflating)", m_pos, m_size, text.c_str());
	m_flags |= Flag::invalid;
}*/

Ex<int> GzipStream::loadData(Span<char> data) {
	PASSERT(m_is_loading);
	if(!m_is_valid) {
		fill(data, 0);
		return ERROR("Reading from invalidated stream");
	}
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
			if(exceptionRaised()) {
				fill(data, 0);
				m_is_valid = false;
				return ERROR("Error while reading data from input stream");
			}

			ctx.avail_in = max_read;
			ctx.next_in = (Bytef *)m_buffer.data();
		}

		ctx.avail_out = data.size() - out_pos;
		ctx.next_out = (Bytef *)data.data() + out_pos;
		auto ret = inflate(&ctx, Z_NO_FLUSH);

		if(isOneOf(ret, Z_STREAM_ERROR, Z_MEM_ERROR, Z_DATA_ERROR)) {
			m_is_valid = m_is_finished = false;
			fill(data, 0);
			return ERROR("Error while inflating data: %", ret);
		}
		out_pos = data.size() - ctx.avail_out;

		if(ret == Z_STREAM_END) {
			m_is_finished = true;
			break;
		}
	}

	return out_pos;
}

Ex<vector<char>> GzipStream::loadData() {
	vector<char> out;
	while(!isFinished()) {
		char buffer[4096];
		if(auto result = loadData(buffer))
			insertBack(out, cspan(buffer, *result));
		else
			return result.error();
	}
	return out;
}

Ex<void> GzipStream::saveData(CSpan<char> data) {
	PASSERT(!m_is_loading && !m_is_finished);
	if(!m_is_valid)
		return ERROR("Writing to invalidated stream");
	if(!data)
		return {};

	auto &ctx = *(z_stream *)m_ctx;
	ctx.avail_in = data.size();
	ctx.next_in = (Bytef*)data.data();
	PASSERT(ctx.avail_out);

	while(ctx.avail_in > 0) {
		auto ret = deflate(&ctx, Z_NO_FLUSH);

		if(isOneOf(ret, Z_STREAM_ERROR, Z_MEM_ERROR, Z_DATA_ERROR)) {
			m_is_valid = false;
			return ERROR("Error while deflating data: %", ret);
		}

		// Flusing buffer
		if(ctx.avail_out == 0) {
			m_pipe->saveData(m_buffer);
			if(exceptionRaised()) {
				m_is_valid = false;
				return ERROR("Error while reading data from input stream");
			}
			ctx.avail_out = m_buffer.size();
			ctx.next_out = (Bytef*)m_buffer.data();
		}
	}

	return {};
}

Ex<void> GzipStream::finish() {
	PASSERT(!m_is_loading && !m_is_finished);
	if(!m_is_valid)
		return ERROR("Writing to invalidated stream");

	auto &ctx = *(z_stream *)m_ctx;
	ctx.avail_in = 0;
	ctx.next_in = nullptr;
	PASSERT(ctx.avail_out);

	int ret = 0;
	do {
		ret = deflate(&ctx, Z_FINISH);

		if(isOneOf(ret, Z_STREAM_ERROR, Z_MEM_ERROR, Z_DATA_ERROR)) {
			m_is_valid = false;
			return ERROR("Error while deflating data: %", ret);
		}

		if(ctx.avail_out < (uint)m_buffer.size() || ret == Z_STREAM_END) {
			m_pipe->saveData(cspan(m_buffer.data(), m_buffer.size() - ctx.avail_out));
			if(exceptionRaised()) {
				m_is_valid = false;
				return ERROR("Error while reading data from input stream");
			}
			ctx.avail_out = m_buffer.size();
			ctx.next_out = (Bytef*)m_buffer.data();
		}
	} while(ret != Z_STREAM_END);
	m_is_finished = true;

	return {};
}
}
