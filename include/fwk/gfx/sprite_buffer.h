// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/material.h"
#include "fwk/math/matrix4.h"

namespace fwk {

class SpriteBuffer {
  public:
	struct Instance {
		Matrix4 matrix;
		Material material;
		vector<float3> positions;
		vector<float2> tex_coords;
		vector<IColor> colors;
	};

	SpriteBuffer(const MatrixStack &);
	void add(CSpan<float3> verts, CSpan<float2> tex_coords, CSpan<IColor> colors, const Material &,
			 const Matrix4 &matrix = Matrix4::identity());
	void clear();

	const auto &instances() const { return m_instances; }

  private:
	Instance &instance(const Material &, Matrix4, bool empty_colors, bool has_tex_coords);

	vector<Instance> m_instances;
	const MatrixStack &m_matrix_stack;
};
}
