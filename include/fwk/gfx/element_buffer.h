// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(ElementBufferOpt, colors, tex_coords);

class ElementBuffer {
  public:
	using Opt = ElementBufferOpt;
	using Flags = EnumFlags<Opt>;

	ElementBuffer(Flags flags = Opt::colors);
	FWK_COPYABLE_CLASS(ElementBuffer);

	// TODO: option to set command_id (simple integer) which can be stored in DrawCall;
	// it can be used to attach additional data to each DrawCall (for example: scissor rects)

	void setMaterial(Material);
	void setTrans(Matrix4);
	void reserve(int num_vertices, int num_elements);
	void clear();

	bool empty() const;
	int size() const;
	vector<Pair<FBox, Matrix4>> drawBoxes() const;

  protected:
	struct Element {
		int first_index, num_vertices;
		int matrix_idx, material_idx;
	};

	FBox bbox(const Element &) const;
	int numVerts(const Element &elem) const {
		return elem.num_vertices == 0 ? m_positions.size() - elem.first_index : elem.num_vertices;
	}
	bool emptyElement() const { return m_elements.back().first_index == m_positions.size(); }

	void addElement();
	vector<DrawCall> drawCalls(PrimitiveType, bool compute_bboxes) const;

	vector<float3> m_positions;
	vector<IColor> m_colors;
	vector<float2> m_tex_coords;
	vector<Element> m_elements;

	vector<Material> m_materials;
	vector<Matrix4> m_matrices;
	int m_matrix_lock = 0, m_material_lock = 0;
	Flags m_flags;
};
}
