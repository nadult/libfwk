// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/multi_draw_call.h"

#include "fwk/gfx/opengl.h"

namespace fwk {

MultiDrawCall::MultiDrawCall(PVertexArray2 vao, PBuffer buffer, PrimitiveType prim, int cmd_count_)
	: vao(move(vao)), cmd_buffer(move(buffer)), cmd_count(cmd_count_), prim_type(prim) {
	DASSERT(this->vao && cmd_buffer);
	DASSERT(cmd_buffer->type() == BufferType::draw_indirect);

	if(cmd_count < 0)
		cmd_count = cmd_buffer->size() / sizeof(DrawIndirectCommand);
	else
		DASSERT(cmd_buffer->size() >= int(cmd_count * sizeof(DrawIndirectCommand)));
}

FWK_COPYABLE_CLASS_IMPL(MultiDrawCall);

void MultiDrawCall::issue() const { vao->drawIndirect(prim_type, cmd_buffer, cmd_count); }
}
