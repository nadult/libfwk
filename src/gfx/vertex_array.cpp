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
							  bind.stride, (void *)(unsigned long)bind.offset);
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
						  (void *)(size_t)offset);
	glEnableVertexAttribArray(m_bind_count);
#else
	ASSERT(m_bind_count < max_binds);

	m_binds[m_bind_count] = {VertexBuffer::activeHandle(), offset, (u16)type, (u16)stride,
							 (u8)m_bind_count, (u8)size, normalize};
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

static const int gl_vertex_data_type[] = {GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT,
										  GL_FLOAT};

static const int gl_index_data_type[] = {GL_UNSIGNED_INT, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT};
static const int gl_primitive_type[] = {GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP};
static_assert(arraySize(gl_primitive_type) == PrimitiveType::count, "");

NiceVertexArray::NiceVertexArray(const vector<VertexBufferSource> &vbs)
	: m_vertex_buffers(vbs) {
	m_size = vbs.empty()? 0 : vbs.front().size();
	for(const auto &vb : m_vertex_buffers)
		m_size = min(m_size, vb.size());
	initArray();
}

NiceVertexArray::NiceVertexArray(const vector<VertexBufferSource> &vbs, PNiceIndexBuffer ib)
	: m_vertex_buffers(vbs), m_index_buffer(ib) {
	DASSERT(ib);
	m_size = ib->size();
	initArray();
}

void NiceVertexArray::drawPrimitives(PrimitiveType::Type pt, int num_vertices, int offset) const {
	if(!num_vertices)
		return;
	DASSERT(offset >= 0 && num_vertices >= 0);
	DASSERT(num_vertices + offset <= m_size);

	m_vertex_array.bind();
	if(m_index_buffer) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer->m_handle);
		glDrawElements(gl_primitive_type[pt], num_vertices,
					   gl_index_data_type[m_index_buffer->m_index_type],
					   (void *)(size_t)(offset * m_index_buffer->indexSize()));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	} else { glDrawArrays(gl_primitive_type[pt], offset, num_vertices); }

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	VertexArray::unbind();
}

void NiceVertexArray::initArray() {
	m_vertex_array.bind();
	for(const auto &source : m_vertex_buffers) {
		VertexBuffer::bindHandle(source.buffer->m_handle);
		m_vertex_array.addAttrib(
			source.buffer->m_data_type.size, gl_vertex_data_type[source.buffer->m_data_type.type],
			source.normalize_data, source.buffer->m_vertex_size, source.offset);
	}
	VertexArray::unbind();
}
}
