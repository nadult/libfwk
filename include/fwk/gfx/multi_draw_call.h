// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"

#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx/gl_vertex_array.h"
#include "fwk/gfx/material.h"
#include "fwk/math/matrix4.h"

namespace fwk {

// TODO: to umożliwia połączenie renderingu wielu różnych meshy
// przy czym niektóre meshe mają wiele instancji!
// Ale aby użyć base_instance w shaderze, to trzeba mieć odp. rozszerzenie
struct DrawIndirectCommand {
	uint count;
	uint instance_count;
	uint first;
	uint base_instance;
};

static_assert(sizeof(DrawIndirectCommand) == 16);

class MultiDrawCall {
  public:
	MultiDrawCall(PVertexArray2, PBuffer, PrimitiveType, int cmd_count = -1,
				  const Material & = Material(), Matrix4 = Matrix4::identity());
	FWK_COPYABLE_CLASS(MultiDrawCall);

	void issue() const;

	Matrix4 matrix;
	Material material;
	PVertexArray2 vao;
	PBuffer cmd_buffer;
	PBuffer matrix_buffer;
	int cmd_count;
	PrimitiveType prim_type;
};
}
