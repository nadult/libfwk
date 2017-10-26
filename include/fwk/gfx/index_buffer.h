// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/sys/immutable_ptr.h"

namespace fwk {

class IndexBuffer : public immutable_base<IndexBuffer> {
  public:
	enum { max_index_value = 65535 };

	IndexBuffer(const vector<int> &indices);
	~IndexBuffer();

	void operator=(const IndexBuffer &) = delete;
	IndexBuffer(const IndexBuffer &) = delete;

	vector<int> getData() const;
	int size() const { return m_size; }

  private:
	enum IndexType {
		type_int,
		type_ubyte,
		type_ushort,
	};

	unsigned m_handle;
	int m_size, m_index_size;
	IndexType m_index_type;
	friend class VertexArray;
};
}
