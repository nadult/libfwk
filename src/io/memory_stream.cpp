// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/memory_stream.h"

#include "fwk/math_base.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"

namespace fwk {
BaseMemoryStream::BaseMemoryStream(CSpan<char> data)
	: Stream(data.size(), true), m_data((char *)data.data()), m_capacity(data.size()) {}

BaseMemoryStream::BaseMemoryStream(Span<char> data)
	: Stream(0, false), m_data(data.data()), m_capacity(data.size()) {}

BaseMemoryStream::BaseMemoryStream(PodVector<char> buffer, bool is_loading)
	: Stream(is_loading ? buffer.size() : 0, is_loading), m_buffer(std::move(buffer)),
	  m_data(m_buffer.data()), m_capacity(buffer.size()) {}

BaseMemoryStream::BaseMemoryStream(BaseMemoryStream &&rhs)
	: Stream(std::move(rhs)), m_buffer(std::move(rhs.m_buffer)), m_data(rhs.m_data),
	  m_capacity(rhs.m_capacity) {
	rhs.free();
}

FWK_MOVE_ASSIGN_RECONSTRUCT(BaseMemoryStream)
BaseMemoryStream::~BaseMemoryStream() = default;

void BaseMemoryStream::free() {
	m_buffer.free();
	m_data = nullptr;
	m_capacity = 0;
	m_pos = m_size = 0;
	m_flags &= ~Flag::invalid;
}

PodVector<char> BaseMemoryStream::extractBuffer() {
	auto out = std::move(m_buffer);
	free();
	return out;
}

void BaseMemoryStream::reserve(int new_capacity) {
	DASSERT(isSaving());

	if(new_capacity <= m_capacity)
		return;
	PodVector<char> new_buffer(vectorInsertCapacity<char>(m_capacity, new_capacity));
	copy(new_buffer, data());
	m_data = new_buffer.data();
	m_capacity = new_buffer.size();
	m_buffer.swap(new_buffer);
}

void BaseMemoryStream::saveData(CSpan<char> data) {
	PASSERT(isSaving());
	// TODO: is Valid ?
	if(m_pos + data.size() > m_capacity)
		reserve(m_pos + data.size());
	memcpy(m_data + m_pos, data.data(), data.size());
	m_pos += data.size();
	m_size = max(m_pos, m_size);
}

void BaseMemoryStream::loadData(Span<char> data) {
	PASSERT(isLoading());

	if(!isValid()) {
		fill(data, 0);
		return;
	}
	if(m_pos + data.size() > m_size) {
		reportError(format("Reading past the end: % + % > %", m_pos, data.size(), m_size));
		fill(data, 0);
		return;
	}

	copy(data, cspan(m_data + m_pos, data.size()));
	m_pos += data.size();
}

string BaseMemoryStream::errorMessage(Str text) const {
	return format("MemoryStream(%) error at position %/%: %", isLoading() ? "loading" : "saving",
				  m_pos, m_size, text);
}

MemoryStream memoryLoader(CSpan<char> data) { return MemoryStream(data); }
MemoryStream memoryLoader(vector<char> vec) {
	PodVector<char> pvec;
	pvec.unsafeSwap(vec);
	return MemoryStream(std::move(pvec), true);
}
MemoryStream memoryLoader(PodVector<char> vec) { return MemoryStream(std::move(vec), true); }

MemoryStream memorySaver(Span<char> buf) { return MemoryStream(buf); }
MemoryStream memorySaver(int capacity) { return MemoryStream(PodVector<char>(capacity), false); }
MemoryStream memorySaver(PodVector<char> buffer) { return MemoryStream(std::move(buffer), false); }

template class TStream<BaseMemoryStream>;
}
