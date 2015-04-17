/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

static unsigned s_active_handle = 0;

int VertexBuffer::activeHandle() { return (int)s_active_handle; }

void VertexBuffer::bindHandle(int handle) {
	s_active_handle = handle;
	glBindBuffer(GL_ARRAY_BUFFER, handle);
}

VertexBuffer::VertexBuffer() {
	m_size = 0;
	glGenBuffers(1, &m_handle);
	testGlError("glGenBuffers");
}
VertexBuffer::~VertexBuffer() {
	if(m_handle) {
		if(s_active_handle == m_handle)
			s_active_handle = 0;
		glDeleteBuffers(1, &m_handle);
	}
}

const VertexBuffer &VertexBuffer::operator=(VertexBuffer &&rhs) {
	if(this != &rhs) {
		if(m_handle)
			glDeleteBuffers(1, &m_handle);
		m_handle = rhs.m_handle;
		rhs.m_handle = 0;
	}
	return *this;
}

void VertexBuffer::bind() const {
	if(s_active_handle != m_handle)
		bindHandle(m_handle);
}

void VertexBuffer::unbind() {
	if(s_active_handle)
		bindHandle(0);
}

const void *VertexBuffer::mapForReading() const {
	bind();
	return glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
}

void *VertexBuffer::mapForWriting() {
	bind();
	return glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
}

void VertexBuffer::unmap() const {
	bind();
	glUnmapBuffer(GL_ARRAY_BUFFER);
}

void VertexBuffer::getData(int offset, int count, void *ptr) const {
	bind();
	DASSERT(offset >= 0 && offset + count < m_size);
	glGetBufferSubData(GL_ARRAY_BUFFER, offset, count, ptr);
	testGlError("glGetBufferSubData");
}

void VertexBuffer::setData(int count, const void *ptr, unsigned usage) {
	bind();
	m_size = count;
	glBufferData(GL_ARRAY_BUFFER, count, ptr, usage);
	testGlError("glBufferData");
}
}
