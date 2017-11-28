// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/element_buffer.h"

namespace fwk {

// Gathers colored triangles and generates DrawCalls
class TriangleBuffer : public ElementBuffer {
  public:
	void operator()(const Triangle3F &, IColor = ColorId::white);
	void operator()(const FBox &, IColor = ColorId::white);
	void operator()(CSpan<Triangle3F>, IColor = ColorId::white);

	template <class T> void operator()(const Triangle<T, 3> &tri, IColor color = ColorId::white) {
		(*this)(Triangle3F(tri), color);
	}

	template <class T> void operator()(const Box<T> &box, IColor color = ColorId::white) {
		(*this)(FBox(box), color);
	}

	template <class T> void operator()(CSpan<Triangle<T, 3>>, IColor = ColorId::white);
	template <class T> void operator()(CSpan<Box<T>>, IColor = ColorId::white);

	vector<DrawCall> drawCalls(bool compute_boxes = false) const;
	void reserve(int num_tris, int num_elem);

	using ElementBuffer::clear;
	using ElementBuffer::setMaterial;
	using ElementBuffer::setTrans;
};
}
