// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/element_buffer.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/render_list.h"
#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"
#include "fwk/sys/assert.h"

namespace fwk {

ElementBuffer::ElementBuffer(Flags flags) : m_flags(flags) { clear(); }

FWK_COPYABLE_CLASS_IMPL(ElementBuffer);

void ElementBuffer::setTrans(Matrix4 mat) {
	if(emptyElement() && m_elements.back().matrix_idx == m_matrix_lock) {
		m_matrices.back() = mat;
		return;
	}
	if(mat != m_matrices.back()) {
		m_matrices.emplace_back(mat);
		addElement();
	}
}

void ElementBuffer::setMaterial(Material mat) {
	if(emptyElement() && m_elements.back().material_idx == m_material_lock) {
		m_materials.back() = mat;
		return;
	}
	if(mat != m_materials.back()) {
		m_materials.emplace_back(mat);
		addElement();
	}
}

void ElementBuffer::reserve(int num_tris, int num_elements) {
	m_positions.reserve(num_tris * 3);
	if(m_flags & Opt::colors)
		m_colors.reserve(num_tris * 3);
	if(m_flags & Opt::tex_coords)
		m_tex_coords.reserve(num_tris * 3);
	m_elements.reserve(num_elements);
}

void ElementBuffer::clear() {
	m_matrices.clear();
	m_materials.clear();
	m_positions.clear();
	m_colors.clear();
	m_tex_coords.clear();
	m_elements.clear();

	m_matrices.emplace_back(Matrix4::identity());
	m_materials.emplace_back(Material());
	m_elements.emplace_back(0, 0, 0, 0);
}

void ElementBuffer::addElement() {
	auto &back = m_elements.back();
	int material_idx = m_materials.size() - 1;
	int matrix_idx = m_matrices.size() - 1;
	if(emptyElement()) {
		back.matrix_idx = matrix_idx;
		back.material_idx = material_idx;
		return;
	}
	back.num_vertices = m_positions.size() - back.first_index;
	m_matrix_lock = m_matrices.size();
	m_material_lock = m_materials.size();
	m_elements.emplace_back(m_positions.size(), 0, matrix_idx, material_idx);
}

vector<pair<FBox, Matrix4>> ElementBuffer::drawBoxes() const {
	vector<pair<FBox, Matrix4>> out;
	out.reserve(m_elements.size());

	for(const auto &elem : m_elements) {
		CSpan<float3> verts(m_positions.data() + elem.first_index, numVerts(elem));
		out.emplace_back(enclose(verts), m_matrices[elem.matrix_idx]);
	}

	return out;
}

FBox ElementBuffer::bbox(const Element &elem) const {
	CSpan<float3> verts(m_positions.data() + elem.first_index, numVerts(elem));
	return encloseTransformed(enclose(verts), m_matrices[elem.matrix_idx]);
}

bool ElementBuffer::empty() const { return size() == 0; }
int ElementBuffer::size() const {
	int out = m_elements.size();
	return emptyElement() ? out - 1 : out;
}

vector<DrawCall> ElementBuffer::drawCalls(PrimitiveType pt, bool compute_bboxes) const {
	vector<DrawCall> out;
	out.reserve(size());

	if(!m_positions)
		return out;

	auto vao = RenderList::makeVao(m_positions, m_colors, m_tex_coords);

	for(const auto &elem : m_elements) {
		int num_verts = numVerts(elem);
		if(!num_verts)
			continue;

		int end_index = num_verts + elem.first_index;
		DASSERT_LE(end_index, m_positions.size());
		if(m_flags & Opt::colors)
			DASSERT_LE(end_index, m_colors.size());
		if(m_flags & Opt::tex_coords)
			DASSERT_LE(end_index, m_tex_coords.size());
#ifdef FWK_CHECK_NANS
		DASSERT(!isnan(m_positions));
#endif

		Maybe<FBox> bbox;
		if(compute_bboxes)
			bbox = this->bbox(elem);
		out.emplace_back(vao, pt, num_verts, elem.first_index, m_materials[elem.material_idx],
						 m_matrices[elem.matrix_idx], bbox);
	}

	return out;
}
}
