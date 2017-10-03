// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/index_buffer.h"
#include "fwk/gfx/vertex_array.h"
#include "fwk/gfx/vertex_buffer.h"

#include "fwk_opengl.h"
#include "fwk_profile.h"
#include <climits>

namespace fwk {

static const int gl_vertex_data_type[] = {GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT,
										  GL_FLOAT};
static const int gl_index_data_type[] = {GL_UNSIGNED_INT, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT};

static const EnumMap<PrimitiveType, int> gl_primitives{
	{GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP}};

#if OPENGL_VERSION < 0x30
int VertexArray::s_max_bind = 0;
#endif

VertexArraySource::VertexArraySource(PVertexBuffer buffer, int offset)
	: m_buffer(move(buffer)), m_offset(offset) {
	DASSERT(m_buffer);
}

VertexArraySource::VertexArraySource(const float4 &value) : m_single_value(value), m_offset(0) {}

int VertexArray::Source::maxSize() const {
	return m_buffer ? max(0, m_buffer->size() - m_offset) : INT_MAX;
}

static int sourcesMaxSize(const vector<VertexArraySource> &sources) {
	int size = sources.empty() ? 0 : INT_MAX;

	for(const auto &source : sources)
		size = min(size, source.maxSize());
	return size;
}

VertexArray::VertexArray(vector<Source> sources, PIndexBuffer ib)
	: m_sources(move(sources)), m_index_buffer(ib) {
	if(ib) {
		m_size = ib->size();
		if(sourcesMaxSize(m_sources) == 0)
			m_size = 0;
	} else {
		m_size = sourcesMaxSize(m_sources);
	}

#if OPENGL_VERSION >= 0x30
	glGenVertexArrays(1, &m_handle);
	glBindVertexArray(m_handle);

	for(int n = 0; n < (int)m_sources.size(); n++)
		bindVertexBuffer(n);
	glBindVertexArray(0);
// TODO: test for errors
#endif
}

VertexArray::~VertexArray() {
#if OPENGL_VERSION >= 0x30
	if(m_handle)
		glDeleteVertexArrays(1, &m_handle);
#endif
}

static int countTriangles(PrimitiveType prim_type, int num_indices) {
	if(prim_type == PrimitiveType::triangles)
		return num_indices / 3;
	if(prim_type == PrimitiveType::triangle_strip)
		return max(0, num_indices - 2);
	return 0;
}

void VertexArray::draw(PrimitiveType pt, int num_vertices, int offset) const {
	if(!num_vertices)
		return;
	DASSERT(offset >= 0 && num_vertices >= 0);
	DASSERT(num_vertices + offset <= m_size);

	bind();

	FWK_PROFILE_COUNTER("Gfx::draw_calls", 1);
	if(m_index_buffer) {
		FWK_PROFILE_COUNTER("Gfx::tris", countTriangles(pt, num_vertices));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer->m_handle);
		glDrawElements(gl_primitives[pt], num_vertices,
					   gl_index_data_type[m_index_buffer->m_index_type],
					   (void *)(size_t)(offset * m_index_buffer->m_index_size));
		testGlError("glDrawElements");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	} else {
		FWK_PROFILE_COUNTER("Gfx::tris", countTriangles(pt, num_vertices));
		glDrawArrays(gl_primitives[pt], offset, num_vertices);
	}

	unbind();
}

void VertexArray::bind() const {
#if OPENGL_VERSION >= 0x30
	glBindVertexArray(m_handle);
#else
	int max_bind = 0;
	for(int n = 0; n < (int)m_sources.size(); n++)
		if(bindVertexBuffer(n))
			max_bind = max(max_bind, n);

	for(int n = (int)m_sources.size(); n <= s_max_bind; n++)
		glDisableVertexAttribArray(n);
	s_max_bind = max_bind;
#endif
}

bool VertexArray::bindVertexBuffer(int n) const {
	const auto &source = m_sources[n];

	if(source.m_buffer) {
		glBindBuffer(GL_ARRAY_BUFFER, source.m_buffer->m_handle);

		// TODO: use glVertexAttribIPointer for integer types?
		glVertexAttribPointer(n, source.m_buffer->m_data_type.size,
							  gl_vertex_data_type[source.m_buffer->m_data_type.type],
							  source.m_buffer->m_data_type.normalize ? GL_TRUE : GL_FALSE,
							  source.m_buffer->m_vertex_size, (void *)(size_t)source.m_offset);
		glEnableVertexAttribArray(n);
		return true;
	} else {
		const float4 &value = source.m_single_value;
		glVertexAttrib4f(n, value.x, value.y, value.z, value.w);
		glDisableVertexAttribArray(n);
		return false;
	}
}

void VertexArray::unbind() {
#if OPENGL_VERSION >= 0x30
	glBindVertexArray(0);
#endif
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
}
