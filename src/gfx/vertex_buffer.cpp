/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

VertexBuffer::VertexBuffer(const void *data, int size, int vertex_size, VertexDataType data_type)
	: m_handle(0), m_size(size), m_vertex_size(vertex_size), m_data_type(data_type) {
	if(!m_size)
		return;

	glGenBuffers(1, &m_handle);
	testGlError("glGenBuffers");

	glBindBuffer(GL_ARRAY_BUFFER, m_handle);
	glBufferData(GL_ARRAY_BUFFER, m_size * m_vertex_size, data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
	
void VertexBuffer::download(Range<char> out, int src_offset) const {
	DASSERT(out.size() + src_offset <= m_size * m_vertex_size);
	glBindBuffer(GL_ARRAY_BUFFER, m_handle);
	glGetBufferSubData(GL_ARRAY_BUFFER, src_offset, out.size(), out.data());
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

VertexBuffer::~VertexBuffer() {
	if(m_handle)
		glDeleteBuffers(1, &m_handle);
}
}
