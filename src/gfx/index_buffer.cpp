/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

IndexBuffer::IndexBuffer(const void *data, int size, int index_size, IndexType index_type)
	: m_handle(0), m_size(size), m_index_size(index_size), m_index_type(index_type) {
	if(!m_size)
		return;

	glGenBuffers(1, &m_handle);
	testGlError("glGenBuffers");

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_handle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_size * m_index_size, data, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

IndexBuffer::~IndexBuffer() {
	if(m_handle)
		glDeleteBuffers(1, &m_handle);
}
}
