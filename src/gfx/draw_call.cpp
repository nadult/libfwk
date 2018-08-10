// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/draw_call.h"

#include "fwk/gfx/opengl.h"
#include "fwk/gfx/vertex_array.h"

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
}
