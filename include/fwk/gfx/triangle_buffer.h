// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/element_buffer.h"

namespace fwk {

// Gathers colored triangles and generates DrawCalls
// 2D primitives span over XY
class TriangleBuffer : public ElementBuffer {
  public:
	TriangleBuffer(Flags = Opt::colors);

	void operator()(const Triangle3F &, IColor = ColorId::white);
	void operator()(const FBox &, IColor = ColorId::white);
	void operator()(const FBox &, const Matrix4 &, IColor = ColorId::white);

	void operator()(CSpan<Triangle3F>, IColor = ColorId::white);
	void operator()(CSpan<FBox>, IColor = ColorId::white);

	void operator()(const FRect &, const FRect &tex_rect, CSpan<IColor, 4>);
	void operator()(const FRect &, const FRect &tex_rect, IColor = ColorId::white);

	void operator()(const Triangle2F &, IColor = ColorId::white);
	void operator()(const FRect &, IColor = ColorId::white);
	void operator()(const IRect &, IColor = ColorId::white);

	// tex_coord & color can be empty
	void quads(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<IColor>);
	void operator()(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<IColor>);
	void operator()(CSpan<float2> pos, CSpan<float2> tex_coord, IColor = ColorId::white);

	// Generates DrawCalls suitable for RenderList
	vector<SimpleDrawCall> drawCalls(bool compute_boxes = false) const;
	void reserve(int num_tris, int num_elem);

	using ElementBuffer::clear;
	using ElementBuffer::setMaterial;
	using ElementBuffer::setTrans;

  private:
	void fillBuffers(IColor);
};
}
