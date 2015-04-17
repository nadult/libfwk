/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

static const VertexArray *s_active_array = nullptr;

VertexArray::VertexArray() {
	m_bind_count = 0;
#if OPENGL_VERSION >= 0x30
	glGenVertexArrays(1, &m_handle);
#endif
}
VertexArray::~VertexArray() {
	if(s_active_array == this)
		unbind();
#if OPENGL_VERSION >= 0x30
	if(m_handle)
		glDeleteVertexArrays(1, &m_handle);
#endif
}

VertexArray::VertexArray(VertexArray &&rhs) {
	m_bind_count = 0;
#if OPENGL_VERSION >= 0x30
	m_handle = 0;
#endif
	operator=(std::move(rhs));
}

void VertexArray::operator=(VertexArray &&rhs) {
	if(this != &rhs) {
#if OPENGL_VERSION >= 0x30
		if(m_handle)
			glDeleteBuffers(1, &m_handle);
		m_handle = rhs.m_handle;
		m_bind_count = rhs.m_bind_count;
		rhs.m_handle = 0;
#else
		m_bind_count = rhs.m_bind_count;
		for(int n = 0; n < m_bind_count; n++)
			m_binds[n] = rhs.m_binds[n];
#endif
		if(s_active_array == &rhs)
			s_active_array = this;
	}
}

void VertexArray::bind() const {
	if(s_active_array == this)
		return;
	s_active_array = this;

#if OPENGL_VERSION >= 0x30
	glBindVertexArray(m_handle);
#else
	for(int n = 0; n < m_bind_count; n++) {
		const AttribBind &bind = m_binds[n];
		VertexBuffer::bindHandle(bind.handle);
		glVertexAttribPointer(bind.index, bind.size, bind.type, bind.normalize ? GL_TRUE : GL_FALSE,
							  bind.stride, (void *)(unsigned long) bind.offset);
		glEnableVertexAttribArray(bind.index);
	}
#endif
}

void VertexArray::unbind() {
	if(s_active_array == nullptr)
		return;
	s_active_array = nullptr;

#if OPENGL_VERSION >= 0x30
	glBindVertexArray(0);
#else
	VertexBuffer::unbind();
#endif
}

void VertexArray::addAttrib(int size, int type, bool normalize, int stride, int offset) {
#if OPENGL_VERSION >= 0x30
	bind();
	// TODO: use glVertexAttribIPointer for integer types?
	glVertexAttribPointer(m_bind_count, size, type, normalize ? GL_TRUE : GL_FALSE, stride,
						  (void *)(size_t) offset);
	glEnableVertexAttribArray(m_bind_count);
#else
	ASSERT(m_bind_count < max_binds);

	m_binds[m_bind_count] = {VertexBuffer::activeHandle(), offset,   (u16)type, (u16)stride,
							 (u8)m_bind_count,			   (u8)size, normalize};
#endif
	m_bind_count++;
}

void VertexArray::clear() {
#if OPENGL_VERSION >= 0x30
	bind();
	for(int n = 0; n < m_bind_count; n++)
		glDisableVertexAttribArray(n);
#else
	if(s_active_array == this)
		unbind();
#endif
	m_bind_count = 0;
}
}
