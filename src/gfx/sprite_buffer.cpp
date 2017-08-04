// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_gfx.h"

namespace fwk {

SpriteBuffer::SpriteBuffer(const MatrixStack &stack) : m_matrix_stack(stack) {}

SpriteBuffer::Instance &SpriteBuffer::instance(const Material &mat, Matrix4 matrix, bool has_colors,
											   bool has_tex_coords) {
	Instance *inst = m_instances.empty() ? nullptr : &m_instances.back();
	matrix = m_matrix_stack.viewMatrix() * matrix;

	if(!inst || has_tex_coords != !inst->tex_coords.empty() ||
	   has_colors != !inst->colors.empty() || mat != inst->material || matrix != inst->matrix) {
		m_instances.emplace_back(Instance());
		inst = &m_instances.back();
		inst->matrix = matrix;
		inst->material = mat;
	}

	return *inst;
}

void SpriteBuffer::add(CSpan<float3> verts, CSpan<float2> tex_coords, CSpan<IColor> colors,
					   const Material &material, const Matrix4 &matrix) {
	DASSERT(tex_coords.empty() || tex_coords.size() == verts.size());
	DASSERT(colors.empty() || colors.size() == verts.size());
	auto &inst = instance(material, matrix, !colors.empty(), !tex_coords.empty());
	insertBack(inst.positions, verts);
	insertBack(inst.tex_coords, tex_coords);
	insertBack(inst.colors, colors);
}

void SpriteBuffer::clear() { m_instances.clear(); }
}
