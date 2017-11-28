// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

class ElementBuffer {
  public:
	ElementBuffer();
	FWK_COPYABLE_CLASS(ElementBuffer);

	void setMaterial(Material);
	void setTrans(Matrix4);
	void reserve(int num_vertices, int num_elements);
	void clear();

	int size() const { return m_elements.size(); }
	vector<pair<FBox, Matrix4>> drawBoxes() const;

  protected:
	struct Element {
		int first_index, num_vertices;
		int matrix_idx, material_idx;
	};

	FBox bbox(const Element &) const;
	int numVerts(const Element &elem) const {
		return elem.num_vertices == 0 ? m_positions.size() - elem.first_index : elem.num_vertices;
	}

	void addElement();
	vector<DrawCall> drawCalls(PrimitiveType, bool compute_bboxes) const;

	vector<float3> m_positions;
	vector<IColor> m_colors;
	vector<Element> m_elements;

	vector<Material> m_materials;
	vector<Matrix4> m_matrices;
};
}
