// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.
//
#pragma once

#include "fwk/gfx/material.h"
#include "fwk/gfx/matrix_stack.h"

namespace fwk {

class LineBuffer {
  public:
	struct Instance {
		Matrix4 matrix;
		vector<float3> positions;
		vector<IColor> colors;
		MaterialFlags material_flags;
		IColor material_color;
	};

	LineBuffer(const MatrixStack &);
	void add(CSpan<float3>, CSpan<IColor>, const Material &,
			 const Matrix4 &matrix = Matrix4::identity());
	void add(CSpan<float3>, const Material &, const Matrix4 &matrix = Matrix4::identity());
	void add(CSpan<float3>, IColor, const Matrix4 &matrix = Matrix4::identity());
	void add(CSpan<Segment3<float>>, const Material &, const Matrix4 &matrix = Matrix4::identity());
	void addBox(const FBox &bbox, IColor color, const Matrix4 &matrix = Matrix4::identity());
	void clear();

	const auto &instances() const { return m_instances; }
	vector<DrawCall> drawCalls() const;

  private:
	Instance &instance(IColor, MaterialFlags, Matrix4, bool has_colors);

	vector<Instance> m_instances;
	const MatrixStack &m_matrix_stack;
};
}
