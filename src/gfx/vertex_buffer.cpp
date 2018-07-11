// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/vertex_buffer.h"
#include "fwk/gfx/opengl.h"

namespace fwk {

VertexBuffer::VertexBuffer(const void *data, int size, int vertex_size, VertexDataType data_type)
	: m_handle(0), m_size(size), m_vertex_size(vertex_size), m_data_type(data_type) {
	PASSERT_GFX_THREAD();
	if(!m_size)
		return;

	glGenBuffers(1, &m_handle);
	testGlError("glGenBuffers");

	glBindBuffer(GL_ARRAY_BUFFER, m_handle);
	glBufferData(GL_ARRAY_BUFFER, m_size * m_vertex_size, data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::download(Span<char> out, int src_offset) const {
	PASSERT_GFX_THREAD();
	DASSERT(out.size() + src_offset <= m_size * m_vertex_size);
	glBindBuffer(GL_ARRAY_BUFFER, m_handle);
	glGetBufferSubData(GL_ARRAY_BUFFER, src_offset, out.size(), out.data());
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

VertexBuffer::~VertexBuffer() {
	PASSERT_GFX_THREAD();
	if(m_handle)
		glDeleteBuffers(1, &m_handle);
}
}
