/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

NiceIndexBuffer::NiceIndexBuffer(const void *data, int size, int index_size, IndexType index_type)
	: m_size(size), m_index_size(index_size), m_index_type(index_type) {
	glGenBuffers(1, &m_handle);
	testGlError("glGenBuffers");

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_handle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_size * m_index_size, data, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

NiceIndexBuffer::~NiceIndexBuffer() {
	if(m_handle)
		glDeleteBuffers(1, &m_handle);
}
}
