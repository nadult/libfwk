// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/element_buffer.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/vertex_array.h"
#include "fwk/gfx/vertex_buffer.h"
#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"
#include "fwk/sys/assert.h"

namespace fwk {

ElementBuffer::ElementBuffer() { clear(); }

FWK_COPYABLE_CLASS_IMPL(ElementBuffer);

void ElementBuffer::setTrans(Matrix4 mat) {
	if(m_elements.back().num_vertices == 0 &&
	   m_elements.back().matrix_idx + 1 == m_matrices.size()) {
		m_matrices.back() = mat;
		return;
	}
	if(mat != m_matrices.back()) {
		m_matrices.emplace_back(mat);
		addElement();
	}
}

void ElementBuffer::setMaterial(Material mat) {
	if(m_elements.back().num_vertices == 0 &&
	   m_elements.back().matrix_idx + 1 == m_materials.size()) {
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
	m_colors.reserve(num_tris * 3);
	m_elements.reserve(num_elements);
}

void ElementBuffer::clear() {
	m_matrices.clear();
	m_materials.clear();
	m_positions.clear();
	m_colors.clear();
	m_elements.clear();

	m_matrices.emplace_back(Matrix4::identity());
	m_materials.emplace_back(Material());
	m_elements.emplace_back(0, 0, 0, 0);
}

void ElementBuffer::addElement() {
	auto &back = m_elements.back();
	int material_idx = m_materials.size() - 1;
	int matrix_idx = m_matrices.size() - 1;
	if(back.num_vertices == 0) {
		back.matrix_idx = matrix_idx;
		back.material_idx = material_idx;
		return;
	}
	back.num_vertices = m_positions.size() - back.first_index;
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

vector<DrawCall> ElementBuffer::drawCalls(PrimitiveType pt, bool compute_bboxes) const {
	vector<DrawCall> out;
	out.reserve(m_elements.size());

	if(m_positions.empty())
		return out;

	auto array =
		VertexArray::make({make_immutable<VertexBuffer>(m_positions),
						   make_immutable<VertexBuffer>(m_colors), VertexArraySource(float2())});

	for(const auto &elem : m_elements) {
		int num_verts = numVerts(elem);
		if(!num_verts)
			continue;

		int end_index = num_verts + elem.first_index;
		DASSERT_LE(end_index, m_positions.size());
		DASSERT_LE(end_index, m_colors.size());
#ifdef FWK_CHECK_NANS
		DASSERT(!isnan(m_positions));
#endif

		Maybe<FBox> bbox;
		if(compute_bboxes)
			bbox = this->bbox(elem);
		out.emplace_back(array, pt, num_verts, elem.first_index, m_materials[elem.material_idx],
						 m_matrices[elem.matrix_idx], bbox);
	}
	return out;
}
}
