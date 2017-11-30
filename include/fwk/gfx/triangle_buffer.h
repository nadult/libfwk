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
	void operator()(const FBox &, const Matrix4 &, IColor = ColorId::white);

	void operator()(CSpan<Triangle3F>, IColor = ColorId::white);
	void operator()(CSpan<FBox>, IColor = ColorId::white);

	vector<DrawCall> drawCalls(bool compute_boxes = false) const;
	void reserve(int num_tris, int num_elem);

	using ElementBuffer::clear;
	using ElementBuffer::setMaterial;
	using ElementBuffer::setTrans;
};
}
