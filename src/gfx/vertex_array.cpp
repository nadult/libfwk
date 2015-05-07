/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <climits>

namespace fwk {

static const int gl_vertex_data_type[] = {GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT,
										  GL_FLOAT};
static const int gl_index_data_type[] = {GL_UNSIGNED_INT, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT};
static const int gl_primitive_type[] = {GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP};
static_assert(arraySize(gl_primitive_type) == PrimitiveType::count, "");

VertexArraySource::VertexArraySource(PVertexBuffer buffer, int offset)
	: m_buffer(std::move(buffer)), m_offset(offset) {
	DASSERT(m_buffer);
}

VertexArraySource::VertexArraySource(const float4 &value) : m_single_value(value), m_offset(0) {}

//TODO: not really max, it's max + 1 ...
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
	DASSERT(ib);
	m_size = ib->size();
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

void VertexArray::draw(PrimitiveType::Type pt, int num_vertices, int offset) const {
	if(!num_vertices)
		return;
	DASSERT(offset >= 0 && num_vertices >= 0);
	DASSERT(num_vertices + offset <= m_size);

	bind();

	if(m_index_buffer) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer->m_handle);
		glDrawElements(gl_primitive_type[pt], num_vertices,
					   gl_index_data_type[m_index_buffer->m_index_type],
					   (void *)(size_t)(offset * m_index_buffer->indexSize()));
		testGlError("glDrawElements");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	} else { glDrawArrays(gl_primitive_type[pt], offset, num_vertices); }

	unbind();
}

void VertexArray::bind() const {
#if OPENGL_VERSION >= 0x30
	glBindVertexArray(m_handle);
#else
	for(int n = 0; n < (int)m_sources.size(); n++)
		bindVertexBuffer(n);
#endif
}

void VertexArray::bindVertexBuffer(int n) const {
	const auto &source = m_sources[n];

	if(source.m_buffer) {
		glBindBuffer(GL_ARRAY_BUFFER, source.m_buffer->m_handle);

		// TODO: use glVertexAttribIPointer for integer types?
		glVertexAttribPointer(n, source.m_buffer->m_data_type.size,
							  gl_vertex_data_type[source.m_buffer->m_data_type.type],
							  source.m_buffer->m_data_type.normalize ? GL_TRUE : GL_FALSE,
							  source.m_buffer->m_vertex_size, (void *)(size_t)source.m_offset);
		glEnableVertexAttribArray(n);
	} else {
		const float4 &value = source.m_single_value;
		glVertexAttrib4f(n, value.x, value.y, value.z, value.w);
		glDisableVertexAttribArray(n);
	}
	// TODO: shouldn't other vertex attrib arrays be disabled?
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
