// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/file_stream.h"

#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"
#include "stream_inl.h"
#include <errno.h>

namespace fwk {

using namespace std;

BaseFileStream::BaseFileStream() : Stream(0, true), m_file(nullptr) {}

BaseFileStream::BaseFileStream(BaseFileStream &&rhs)
	: Stream(move(rhs)), m_name(move(rhs.m_name)), m_file(rhs.m_file) {
	rhs.m_file = nullptr;
	rhs.m_flags |= Flag::invalid;
}

FWK_MOVE_ASSIGN_RECONSTRUCT(BaseFileStream);

BaseFileStream::~BaseFileStream() {
	if(m_file)
		fclose((FILE *)m_file);
}

void BaseFileStream::raise(ZStr text) {
	RAISE("FileStream '%' % error at position %/%: %", m_name, isLoading() ? "loading" : "saving",
		  m_pos, m_size, text);
	m_flags |= Flag::invalid;
}

void BaseFileStream::loadData(Span<char> data) {
	PASSERT(isLoading());
	if(!isValid() || !data) {
		fill(data, 0);
		return;
	}

	if(m_pos + data.size() > m_size) {
		raise(format("Reading past the end: % + % > %", m_pos, data.size(), m_size));
		fill(data, 0);
		return;
	}

	if(fread(data.data(), data.size(), 1, (FILE *)m_file) != 1) {
		raise(format("fread failed: %", strerror(errno)));
		fill(data, 0);
		return;
	}

	m_pos += data.size();
}

void BaseFileStream::saveData(CSpan<char> data) {
	PASSERT(isSaving());
	if(!isValid() || !data)
		return;

	if(fwrite(data.data(), data.size(), 1, (FILE *)m_file) != 1) {
		raise(format("fwrite failed: %", strerror(errno)));
		return;
	}

	m_pos += data.size();
	if(m_pos > m_size)
		m_size = m_pos;
}

void BaseFileStream::seek(long long pos) {
	DASSERT(pos >= 0 && pos <= m_size);
	if(!isValid())
		return;

	if(fseek((FILE *)m_file, pos, SEEK_SET) != 0) {
		raise(format("fseek failed: %", strerror(errno)));
		return;
	}

	m_pos = pos;
}

Ex<FileStream> fileStream(ZStr file_name, bool is_loading) {
	auto *file = fopen(file_name.c_str(), is_loading ? "rb" : "wb");
	if(!file)
		return FWK_ERROR("Error while opening file \"%\"", file_name);

	fseek(file, 0, SEEK_END);
	auto size = ftell(file);

	// TODO: that's not a proper way to detect a directory
	if(size < -1) {
		fclose(file);
		return FWK_ERROR("Trying to open a directory: \"%\"", file_name);
	}
	fseek(file, 0, SEEK_SET);

	BaseFileStream out;
	out.m_name = file_name;
	out.m_file = file;
	out.m_size = size;
	out.m_pos = 0;
	out.m_flags = mask(is_loading, StreamFlag::loading);
	return move(reinterpret_cast<FileStream &>(out)); // TODO: hmmm
}

Ex<FileStream> fileLoader(ZStr file_name) { return fileStream(file_name, true); }
Ex<FileStream> fileSaver(ZStr file_name) { return fileStream(file_name, false); }

template class TStream<BaseFileStream>;
}
