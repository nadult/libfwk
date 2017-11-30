// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.
//
#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/element_buffer.h"

namespace fwk {

// Gathers colored lines and generates DrawCalls
class LineBuffer : public ElementBuffer {
  public:
	void operator()(CSpan<float3>, CSpan<IColor>);
	void operator()(CSpan<float3>, IColor = ColorId::white);
	void operator()(CSpan<Segment3<float>>, CSpan<IColor>);
	void operator()(CSpan<Segment3<float>>, IColor = ColorId::white);
	void operator()(CSpan<FBox>, IColor = ColorId::white);
	void operator()(CSpan<Triangle3F>, IColor = ColorId::white);
	void operator()(const Segment3<float> &, IColor = ColorId::white);
	void operator()(const FBox &, IColor color = ColorId::white);
	void operator()(const FBox &, const Matrix4 &, IColor color = ColorId::white);
	void operator()(const Triangle3F &, IColor color = ColorId::white);

	vector<DrawCall> drawCalls(bool compute_boxes = false) const;

	void reserve(int num_lines, int num_elements);

	using ElementBuffer::clear;
	using ElementBuffer::setMaterial;
	using ElementBuffer::setTrans;
};
}
