// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/multi_draw_call.h"

#include "fwk/gfx/opengl.h"

namespace fwk {

MultiDrawCall::MultiDrawCall(PVertexArray2 vao, PBuffer buffer, PrimitiveType prim, int cmd_count_,
							 const Material &material, Matrix4 matrix)
	: matrix(matrix), material(material), vao(move(vao)), cmd_buffer(move(buffer)),
	  cmd_count(cmd_count_), prim_type(prim) {
	DASSERT(this->vao && cmd_buffer);
	DASSERT(cmd_buffer->type() == BufferType::draw_indirect);

	if(cmd_count < 0)
		cmd_count = cmd_buffer->size() / sizeof(DrawIndirectCommand);
	else
		DASSERT(cmd_buffer->size() >= int(cmd_count * sizeof(DrawIndirectCommand)));
}

FWK_COPYABLE_CLASS_IMPL(MultiDrawCall);

void MultiDrawCall::issue() const {
	vao->bind();
	//int num_verts = vao->buffers()[0]->size() / sizeof(float3);
	//vao->draw(PrimitiveType::triangles, num_verts, 0);
	cmd_buffer->bind();
	glMultiDrawArraysIndirect(GL_TRIANGLES, nullptr, cmd_count, sizeof(DrawIndirectCommand));
	cmd_buffer->unbind();
}
}
