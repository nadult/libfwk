// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_vertex_array.h"

#include "fwk/enum_map.h"
#include "fwk/format.h"
#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx/opengl.h"
#include <climits>

namespace fwk {

VertexAttrib::VertexAttrib(DataType type, int size, int padding, Flags flags)
	: type(type), size(size), padding(padding), flags(flags) {
	if(flags & (Opt::normalized | Opt::as_integer))
		PASSERT(isIntegral(type));
	if(flags & Opt::normalized)
		PASSERT(!(flags & Opt::as_integer));
	PASSERT(size >= 1 && size <= 255);
	PASSERT(padding >= 0 && padding <= 255);
}

struct VertexDataInfo {
	int size;
	int gl_type;
};

static const EnumMap<VertexBaseType, VertexDataInfo> data_info = {
	{1, GL_BYTE}, {1, GL_UNSIGNED_BYTE}, {2, GL_SHORT},		 {2, GL_UNSIGNED_SHORT},
	{4, GL_INT},  {4, GL_UNSIGNED_INT},  {2, GL_HALF_FLOAT}, {4, GL_FLOAT}};

int dataSize(VertexBaseType type) { return data_info[type].size; }

static const EnumMap<IndexType, int> gl_index_type{GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT,
												   GL_UNSIGNED_INT};

int dataSize(IndexType itype) {
	return itype == IndexType::uint8 ? 1 : itype == IndexType::uint16 ? 2 : 4;
}

static const EnumMap<PrimitiveType, int> gl_primitives{
	{GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP}};

GlVertexArray::GlVertexArray() : m_has_vao(gl_info->hasFeature(GlFeature::vertex_array_object)) {
	if(!m_has_vao) {
		FATAL("check if this mode works");
		// TODO: is it possible to render without VAO in Opengl > 3.0 ?
		// is this even an option ?
	}
}
GlVertexArray::GlVertexArray(GlVertexArray &&) = default;
GlVertexArray::~GlVertexArray() = default;

void GlVertexArray::set(CSpan<PBuffer> buffers, CSpan<VertexAttrib> attribs) {
	m_num_attribs = buffers.size();
	DASSERT(buffers.size() == attribs.size());
	DASSERT(buffers.size() <= max_attribs);
	for(auto &buffer : buffers)
		DASSERT(buffer && buffers[0]->type() == BufferType::array);
	copy(m_vertex_buffers, buffers);
	copy(m_attribs, attribs);
	fill();
}

void GlVertexArray::set(CSpan<PBuffer> buffers, CSpan<VertexAttrib> attribs, PBuffer ibuffer,
						IndexType itype) {
	set(buffers, attribs);
	setIndices(ibuffer, itype);
}

void GlVertexArray::setIndices(PBuffer ibuffer, IndexType itype) {
	m_index_buffer = move(ibuffer);
	m_index_type = itype;
	DASSERT(m_index_buffer && m_index_buffer->type() == BufferType::element_array);
}

void GlVertexArray::fill() {
	if(!m_has_vao)
		return;

	glBindVertexArray(id());
	for(int n : intRange(m_num_attribs))
		bindVertexBuffer(n);
	glBindVertexArray(0);
}

int GlVertexArray::size() const {
	if(m_index_buffer)
		return m_index_buffer->size() / dataSize(m_index_type);
	DASSERT(m_num_attribs > 0);
	return m_vertex_buffers[0]->size() / m_attribs[0].dataSize();
}

static int countTriangles(PrimitiveType prim_type, int num_indices) {
	if(prim_type == PrimitiveType::triangles)
		return num_indices / 3;
	if(prim_type == PrimitiveType::triangle_strip)
		return max(0, num_indices - 2);
	return 0;
}

void GlVertexArray::draw(PrimitiveType pt, int num_elements, int element_offset) const {
	if(!num_elements)
		return;
	DASSERT(element_offset >= 0 && num_elements >= 0);
	// TODO: more checks
	DASSERT(num_elements + element_offset <= size());

	bind();

	if(m_index_buffer) {
		int offset = element_offset * dataSize(m_index_type);
		m_index_buffer->bind();
		glDrawElements(gl_primitives[pt], num_elements, gl_index_type[m_index_type],
					   (void *)(long long)offset);
		m_index_buffer->unbind();
	} else {
		glDrawArrays(gl_primitives[pt], element_offset, num_elements);
	}
}

void GlVertexArray::drawInstanced(PrimitiveType pt, int num_elements, int num_instances,
								  int offset) {
	if(!num_elements)
		return;
	DASSERT(offset >= 0 && num_elements >= 0);
	// TODO: more checks
	DASSERT(num_elements + offset <= size());

	bind();

	if(m_index_buffer) {
		m_index_buffer->bind();
		glDrawElementsInstanced(gl_primitives[pt], num_elements, gl_index_type[m_index_type],
								(void *)(long long)offset, num_instances);
		m_index_buffer->unbind();
	} else {
		glDrawArraysInstanced(gl_primitives[pt], offset, num_elements, num_instances);
	}
}

void GlVertexArray::drawIndirect(PrimitiveType pt, PBuffer cmd_buffer, int num_commands,
								 int offset) const {
	DASSERT(cmd_buffer && cmd_buffer->type() == BufferType::draw_indirect);

	if(num_commands == -1)
		num_commands = cmd_buffer->size<DrawIndirectCommand>();
	if(m_index_buffer)
		FATAL("write me");

	bind();
	// TODO: how to count triangles ?
	cmd_buffer->bind();
	glMultiDrawArraysIndirect(GL_TRIANGLES, (void *)(long long)offset, num_commands,
							  sizeof(DrawIndirectCommand));
	cmd_buffer->unbind();
}

void GlVertexArray::bind() const {
	if(m_has_vao) {
		glBindVertexArray(id());
	} else {
		// TODO: s_max_bind ?
		for(int n : intRange(m_num_attribs))
			bindVertexBuffer(n);
		for(int n = m_num_attribs; n < max_attribs; n++)
			glDisableVertexAttribArray(n);
	}
}

void GlVertexArray::bindVertexBuffer(int n) const {
	glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffers[n]->id());

	auto &attrib = m_attribs[n];
	auto gl_type = data_info[attrib.type].gl_type;

	if(attrib.flags & VertexAttribOpt::as_integer)
		glVertexAttribIPointer(n, attrib.size, gl_type, attrib.dataSize(), 0);
	else {
		auto normalized = attrib.flags & VertexAttribOpt::normalized ? GL_TRUE : GL_FALSE;
		glVertexAttribPointer(n, attrib.size, gl_type, normalized, attrib.dataSize(), 0);
	}
	glEnableVertexAttribArray(n);
}

void GlVertexArray::unbind() {
	if(gl_info->hasFeature(GlFeature::vertex_array_object))
		glBindVertexArray(0);
	// TODO: this is wrong (what about disabling attribs?)
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
}
