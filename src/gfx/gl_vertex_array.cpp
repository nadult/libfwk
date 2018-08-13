// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_vertex_array.h"

#include "fwk/enum_map.h"
#include "fwk/gfx/index_buffer.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/vertex_buffer.h"
#include "fwk_profile.h"
#include <climits>

namespace fwk {

struct VertexDataInfo {
	int size;
	int gl_type;
};

static const EnumMap<VertexDataType_, VertexDataInfo> data_info = {
	{1, GL_BYTE}, {1, GL_UNSIGNED_BYTE}, {2, GL_SHORT},		 {2, GL_UNSIGNED_SHORT},
	{4, GL_INT},  {4, GL_UNSIGNED_INT},  {2, GL_HALF_FLOAT}, {4, GL_FLOAT}};

int dataSize(VertexDataType_ type) { return data_info[type].size; }

static const int gl_index_data_type[] = {GL_UNSIGNED_INT, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT};

static const EnumMap<PrimitiveType, int> gl_primitives{
	{GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP}};

GlVertexArray::GlVertexArray()
	: m_has_vao(opengl_info->hasFeature(OpenglFeature::vertex_array_object)) {
	DASSERT(storage.contains(this));
}
GlVertexArray::~GlVertexArray() = default;

void GlVertexArray::set(CSpan<PBuffer> buffers, CSpan<VertexAttrib> attribs) {
	m_size = buffers.size();
	DASSERT(buffers.size() == attribs.size());
	DASSERT(buffers.size() <= max_attribs);
	for(auto &buffer : buffers)
		DASSERT(buffer && buffers[0]->type() == BufferType::array);
	copy(m_vertex_buffers, buffers);
	copy(m_attribs, attribs);
	fill();
}

void GlVertexArray::set(CSpan<PBuffer> buffers, CSpan<VertexAttrib> attribs, PBuffer ibuffer,
						IndexDataType itype) {
	set(buffers, attribs);
	m_index_buffer = move(ibuffer);
	m_index_data_type = itype;
	DASSERT(m_index_buffer && m_index_buffer->type() == BufferType::element_array);
}

void GlVertexArray::fill() {
	if(!m_has_vao)
		return;

	glBindVertexArray(id());
	for(int n : intRange(m_size))
		bindVertexBuffer(n);
	glBindVertexArray(0);
}

static int countTriangles(PrimitiveType prim_type, int num_indices) {
	if(prim_type == PrimitiveType::triangles)
		return num_indices / 3;
	if(prim_type == PrimitiveType::triangle_strip)
		return max(0, num_indices - 2);
	return 0;
}

void GlVertexArray::draw(PrimitiveType pt, int num_vertices, int offset) const {
	if(!num_vertices)
		return;
	DASSERT(offset >= 0 && num_vertices >= 0);
	//DASSERT(num_vertices + offset <= m_size); // TODO: different size

	bind();

	FWK_PROFILE_COUNTER("Gfx::draw_calls", 1);
	if(m_index_buffer) {
		FATAL("Writeme");
		/*
		FWK_PROFILE_COUNTER("Gfx::tris", countTriangles(pt, num_vertices));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer->m_handle);
		glDrawElements(gl_primitives[pt], num_vertices,
					   gl_index_data_type[m_index_buffer->m_index_type],
					   (void *)(size_t)(offset * m_index_buffer->m_index_size));
		testGlError("glDrawElements");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);*/
	} else {
		FWK_PROFILE_COUNTER("Gfx::tris", countTriangles(pt, num_vertices));
		glDrawArrays(gl_primitives[pt], offset, num_vertices);
	}
}

void GlVertexArray::bind() const {
	if(m_has_vao) {
		glBindVertexArray(id());
	} else {
		// TODO: s_max_bind ?
		for(int n : intRange(m_size))
			bindVertexBuffer(n);
		for(int n = m_size; n < max_attribs; n++)
			glDisableVertexAttribArray(n);
	}
}

void GlVertexArray::bindVertexBuffer(int n) const {
	glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffers[n]->id());

	auto &attrib = m_attribs[n];
	glVertexAttribPointer(n, attrib.size, data_info[attrib.type].gl_type,
						  attrib.normalized ? GL_TRUE : GL_FALSE, attrib.dataSize(), 0);
	glEnableVertexAttribArray(n);
}

void GlVertexArray::unbind() {
	if(opengl_info->hasFeature(OpenglFeature::vertex_array_object))
		glBindVertexArray(0);
	// TODO: this is wrong (what about disabling attribs?)
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
}
