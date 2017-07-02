// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_gfx.h"

namespace fwk {

LineBuffer::LineBuffer(const MatrixStack &stack) : m_matrix_stack(stack) {}

LineBuffer::Instance &LineBuffer::instance(FColor col, uint flags, Matrix4 matrix,
										   bool has_colors) {
	Instance *inst = m_instances.empty() ? nullptr : &m_instances.back();
	matrix = m_matrix_stack.viewMatrix() * matrix;

	if(!inst || has_colors != !inst->colors.empty() || col != inst->material_color ||
	   flags != inst->material_flags || matrix != inst->matrix) {
		m_instances.emplace_back(Instance());
		inst = &m_instances.back();
		inst->matrix = matrix;
		inst->material_color = col;
		inst->material_flags = flags;
	}

	return *inst;
}

void LineBuffer::add(CRange<float3> verts, CRange<IColor> colors, PMaterial mat,
					 const Matrix4 &matrix) {
	DASSERT(verts.size() % 2 == 0);
	DASSERT(colors.size() == verts.size() || colors.empty());
	DASSERT(mat);

	auto &inst = instance(mat->color(), mat->flags(), matrix, true);
	insertBack(inst.positions, verts);
	insertBack(inst.colors, colors);
}

void LineBuffer::add(CRange<float3> verts, PMaterial material, const Matrix4 &matrix) {
	DASSERT(verts.size() % 2 == 0);
	DASSERT(material);

	auto &inst = instance(material->color(), material->flags(), matrix, false);
	insertBack(inst.positions, verts);
}

void LineBuffer::add(CRange<float3> verts, IColor color, const Matrix4 &matrix) {
	DASSERT(verts.size() % 2 == 0);
	auto &inst = instance(color, 0u, matrix, false);
	insertBack(inst.positions, verts);
}

void LineBuffer::add(CRange<Segment3<float>> segs, PMaterial material, const Matrix4 &matrix) {
	vector<float3> verts;
	verts.reserve(segs.size() * 2);
	for(const auto &seg : segs) {
		verts.emplace_back(seg.from);
		verts.emplace_back(seg.to);
	}
	add(verts, material, matrix);
}

void LineBuffer::addBox(const FBox &bbox, IColor color, const Matrix4 &matrix) {
	auto verts = bbox.corners();
	int indices[] = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 3, 7, 2, 6};
	float3 out_verts[arraySize(indices)];
	for(int i = 0; i < arraySize(indices); i++)
		out_verts[i] = verts[indices[i]];
	add(out_verts, color, matrix);
}

void LineBuffer::clear() { m_instances.clear(); }
}
