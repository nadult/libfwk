// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/draw_call.h"

#include "fwk/gfx/gl_vertex_array.h"
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

DrawCall::DrawCall(PVertexArray vao, PrimitiveType primitive_type, int vertex_count,
				   int index_offset, const Material &material, Matrix4 matrix, Maybe<FBox> bbox)
	: matrix(matrix), bbox(bbox), material(material), vao(move(vao)),
	  primitive_type(primitive_type), vertex_count(vertex_count), index_offset(index_offset) {
	DASSERT(this->vao);
}

FWK_COPYABLE_CLASS_IMPL(DrawCall);

void DrawCall::issue() const { vao->draw(primitive_type, vertex_count, index_offset); }
int DrawCall::primitiveCount() const { return fwk::primitiveCount(primitive_type, vertex_count); }
}
