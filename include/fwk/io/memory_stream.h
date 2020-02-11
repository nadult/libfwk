// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/io/stream.h"
#include "fwk/pod_vector.h"

namespace fwk {

class BaseMemoryStream : public Stream {
  public:
	// Do not call directly, use memoryLoader & memorySaver functions
	BaseMemoryStream(CSpan<char>);
	BaseMemoryStream(Span<char>);
	BaseMemoryStream(PodVector<char>, bool is_loading);

	BaseMemoryStream(BaseMemoryStream &&);
	BaseMemoryStream &operator=(BaseMemoryStream &&);
	~BaseMemoryStream();

	void free();
	// Returns buffer and clears
	PodVector<char> extractBuffer();
	bool bufferUsed() const { return m_data == m_buffer.data(); }

	// Makes sense only for saving streams
	void reserve(int new_capacity);

	CSpan<char> data() const { return {m_data, (int)m_size}; }
	int capacity() const { return m_capacity; }
	int capacityLeft() const { return m_capacity - (int)m_size; }

	void saveData(CSpan<char>) final;
	void loadData(Span<char>) EXCEPT final;

  private:
	PodVector<char> m_buffer;
	char *m_data;
	int m_capacity = 0;
};

// Will keep reference to passed buffer
MemoryStream memoryLoader(CSpan<char>);
MemoryStream memoryLoader(vector<char>);
MemoryStream memoryLoader(PodVector<char>);

// Will keep reference to passed buffer. Will allocate memory when saved data
// won't fit in passed buffer.
MemoryStream memorySaver(Span<char>);
// Data in buffer will be lost
MemoryStream memorySaver(int capacity = 256);
MemoryStream memorySaver(PodVector<char> buffer);
}
