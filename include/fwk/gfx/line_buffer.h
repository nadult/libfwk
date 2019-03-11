// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.
//
#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/element_buffer.h"

namespace fwk {

// Gathers colored lines and generates DrawCalls
// 2D primitives span over XY
class LineBuffer : public ElementBuffer {
  public:
	LineBuffer();

	void operator()(CSpan<float3>, CSpan<IColor>);
	void operator()(CSpan<float3>, IColor = ColorId::white);
	void operator()(CSpan<Segment3F>, CSpan<IColor>);
	void operator()(CSpan<Segment3F>, IColor = ColorId::white);
	void operator()(CSpan<FBox>, IColor = ColorId::white);
	void operator()(CSpan<Triangle3F>, IColor = ColorId::white);
	void operator()(const Segment3F &, IColor = ColorId::white);
	void operator()(const FBox &, IColor color = ColorId::white);
	void operator()(const FBox &, const Matrix4 &, IColor color = ColorId::white);
	void operator()(const Triangle3F &, IColor color = ColorId::white);

	// TODO: more/less overloads ?
	void operator()(const FRect &, IColor = ColorId::white);
	void operator()(const IRect &, IColor = ColorId::white);
	void operator()(const Triangle2F &, IColor = ColorId::white);

	void operator()(const Segment2F, IColor = ColorId::white);
	void operator()(const Segment2I, IColor = ColorId::white);

	void operator()(CSpan<float2> pos, CSpan<IColor>);
	void operator()(CSpan<float2> pos, IColor = ColorId::white);

	// Generates DrawCalls suitable for RenderList
	vector<DrawCall> drawCalls(bool compute_boxes = false) const;

	void reserve(int num_lines, int num_elements);

	using ElementBuffer::clear;
	using ElementBuffer::setMaterial;
	using ElementBuffer::setTrans;
};
}
