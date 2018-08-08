// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/material.h"
#include "fwk/gfx_base.h"
#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"

namespace fwk {

class DrawCall {
  public:
	DrawCall(PVertexArray, PrimitiveType, int vertex_count, int index_offset,
			 const Material & = Material(), Matrix4 = Matrix4::identity(), Maybe<FBox> = none);
	FWK_COPYABLE_CLASS(DrawCall);

	void issue() const;

	Matrix4 matrix;
	Maybe<FBox> bbox; // transformed by matrix
	Material material;

	auto primitiveType() const { return m_primitive_type; }
	int primitiveCount() const;

	PVertexArray m_vertex_array;
	PrimitiveType m_primitive_type;
	int m_vertex_count, m_index_offset;
};

// TODO: to umożliwia połączenie renderingu wielu różnych meshy
// przy czym niektóre meshe mają wiele instancji!
// Ale aby użyć base_instance w shaderze, to trzeba mieć odp. rozszerzenie
struct DrawIndirectCommand {
	uint count;
	uint instance_count;
	uint first;
	uint base_instance;
};

class MultiDrawCall {
  public:
	MultiDrawCall(PVertexArray, SBuffer, PrimitiveType, int cmd_count = -1,
				  const Material & = Material(), Matrix4 = Matrix4::identity());
	FWK_COPYABLE_CLASS(MultiDrawCall);

	void issue() const;

	Matrix4 matrix;
	Material material;
	PVertexArray vao;
	SBuffer cmd_buffer;
	int cmd_count;
	PrimitiveType prim_type;
};
}
