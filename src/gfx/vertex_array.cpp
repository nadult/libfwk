/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include "fwk_profile.h"
#include <climits>

namespace fwk {

static const int gl_vertex_data_type[] = {GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT,
										  GL_FLOAT};
static const int gl_index_data_type[] = {GL_UNSIGNED_INT, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT};
static const int gl_primitive_type[] = {GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP};

#if OPENGL_VERSION < 0x30
int VertexArray::s_max_bind = 0;
#endif

static_assert(arraySize(gl_primitive_type) == PrimitiveType::count, "");

VertexArraySource::VertexArraySource(PVertexBuffer buffer, int offset)
	: m_buffer(std::move(buffer)), m_offset(offset) {
	DASSERT(m_buffer);
}

VertexArraySource::VertexArraySource(const float4 &value) : m_single_value(value), m_offset(0) {}

// TODO: not really max, it's max + 1 ...
int VertexArray::Source::maxIndex() const {
	return m_buffer ? max(0, m_buffer->size() - m_offset) : INT_MAX;
}

static int sourcesMaxIndex(const vector<VertexArraySource> &sources) {
	int size = sources.empty() ? 0 : sources.front().maxIndex();
	for(const auto &source : sources)
		size = min(size, source.maxIndex());
	return size;
}

VertexArray::VertexArray(vector<Source> sources, PIndexBuffer ib)
	: m_sources(std::move(sources)), m_index_buffer(ib) {
	if(ib) {
		m_size = ib->size();
		if(sourcesMaxIndex(m_sources) == 0)
			m_size = 0;
	} else { m_size = sourcesMaxIndex(m_sources); }

#if OPENGL_VERSION >= 0x30
	glGenVertexArrays(1, &m_handle);
	glBindVertexArray(m_handle);

	for(int n = 0; n < (int)m_sources.size(); n++)
		bindVertexBuffer(n);
	glBindVertexArray(0);
#endif
}

VertexArray::~VertexArray() {
#if OPENGL_VERSION >= 0x30
	if(m_handle)
		glDeleteVertexArrays(1, &m_handle);
#endif
}

static int countTriangles(PrimitiveType::Type prim_type, int num_indices) {
	if(prim_type == PrimitiveType::triangles)
		return num_indices / 3;
	if(prim_type == PrimitiveType::triangle_strip)
		return max(0, num_indices - 2);
	return 0;
}

void VertexArray::draw(PrimitiveType::Type pt, int num_vertices, int offset) const {
	if(!num_vertices)
		return;
	DASSERT(offset >= 0 && num_vertices >= 0);
	DASSERT(num_vertices + offset <= m_size);

	bind();

	FWK_PROFILE_COUNTER("gfx::draw_calls", 1);
	if(m_index_buffer) {
		FWK_PROFILE_COUNTER("gfx::tris", countTriangles(pt, num_vertices));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer->m_handle);
		glDrawElements(gl_primitive_type[pt], num_vertices,
					   gl_index_data_type[m_index_buffer->m_index_type],
					   (void *)(size_t)(offset * m_index_buffer->m_index_size));
		testGlError("glDrawElements");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	} else {
		FWK_PROFILE_COUNTER("gfx::tris", countTriangles(pt, num_vertices));
		glDrawArrays(gl_primitive_type[pt], offset, num_vertices);
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

DrawCall::DrawCall(PVertexArray vertex_array, PrimitiveType::Type primitive_type, int vertex_count,
				   int index_offset)
	: m_vertex_array(std::move(vertex_array)), m_primitive_type(primitive_type),
	  m_vertex_count(vertex_count), m_index_offset(index_offset) {
	DASSERT(PrimitiveType::isValid(primitive_type));
	DASSERT(m_vertex_array);
}

void DrawCall::issue() const {
	m_vertex_array->draw(m_primitive_type, m_vertex_count, m_index_offset);
}
}
