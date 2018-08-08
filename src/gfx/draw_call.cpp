// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/vertex_array.h"

#include "fwk/gfx/buffer.h"
#include "fwk/gfx/opengl.h"

namespace fwk {

static int primitiveCount(PrimitiveType type, int vertex_count) {
	switch(type) {
	case PrimitiveType::triangles:
		return vertex_count / 3;
	case PrimitiveType::triangle_strip:
		return max(0, vertex_count - 2);
	case PrimitiveType::lines:
		return vertex_count / 2;
	case PrimitiveType::points:
		return vertex_count;
	}
	return 0;
}

DrawCall::DrawCall(PVertexArray vertex_array, PrimitiveType primitive_type, int vertex_count,
				   int index_offset, const Material &material, Matrix4 matrix, Maybe<FBox> bbox)
	: matrix(matrix), bbox(bbox), material(material), m_vertex_array(move(vertex_array)),
	  m_primitive_type(primitive_type), m_vertex_count(vertex_count), m_index_offset(index_offset) {
	DASSERT(m_vertex_array);
}

FWK_COPYABLE_CLASS_IMPL(DrawCall);

void DrawCall::issue() const {
	m_vertex_array->draw(m_primitive_type, m_vertex_count, m_index_offset);
}

int DrawCall::primitiveCount() const {
	return fwk::primitiveCount(m_primitive_type, m_vertex_count);
}

MultiDrawCall::MultiDrawCall(PVertexArray vao, SBuffer buffer, PrimitiveType prim, int cmd_count_,
							 const Material &material, Matrix4 matrix)
	: matrix(matrix), material(material), vao(move(vao)), cmd_buffer(move(buffer)),
	  cmd_count(cmd_count_), prim_type(prim) {
	DASSERT(vao && cmd_buffer);
	DASSERT(cmd_buffer->type() == BufferType::draw_indirect);
	if(cmd_count < 0)
		cmd_count = cmd_buffer->size() / sizeof(DrawIndirectCommand);
}

FWK_COPYABLE_CLASS_IMPL(MultiDrawCall);

void MultiDrawCall::issue() const {
	vao->bind();
	cmd_buffer->bind();
	glMultiDrawArraysIndirect(GL_TRIANGLES, nullptr, cmd_count, sizeof(DrawIndirectCommand));
	cmd_buffer->unbind();
}
}
