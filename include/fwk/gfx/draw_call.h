// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/material.h"
#include "fwk/gfx_base.h"
#include "fwk_math.h"

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

  private:
	PVertexArray m_vertex_array;
	PrimitiveType m_primitive_type;
	int m_vertex_count, m_index_offset;
};
}