// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/line_buffer.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/math/box.h"
#include "fwk/math/segment.h"
#include "fwk/sys/assert.h"

namespace fwk {

void LineBuffer::operator()(CSpan<float3> verts, CSpan<IColor> colors) {
	DASSERT(verts.size() % 2 == 0);
	DASSERT_EQ(colors.size(), verts.size());
	insertBack(m_positions, verts);
	insertBack(m_colors, colors);
}

void LineBuffer::operator()(CSpan<float3> verts, IColor color) {
	DASSERT(verts.size() % 2 == 0);
	insertBack(m_positions, verts);
	m_colors.resize(m_colors.size() + verts.size(), color);
}

void LineBuffer::operator()(CSpan<Segment3F> segs, CSpan<IColor> colors) {
	static_assert(sizeof(Segment3F) == sizeof(float3) * 2);
	insertBack(m_positions, reinterpret<float3>(segs));
	m_colors.reserve(m_colors.size() + colors.size() * 2);
	for(auto col : colors)
		insertBack(m_colors, {col, col});
}

void LineBuffer::operator()(CSpan<Segment3<float>> segs, IColor color) {
	(*this)(reinterpret<float3>(segs), color);
}

void LineBuffer::operator()(const FBox &bbox, IColor color) {
	auto corners = bbox.corners();
	int indices[] = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 3, 7, 2, 6};
	float3 verts[arraySize(indices)];
	for(int i = 0; i < arraySize(indices); i++)
		verts[i] = corners[indices[i]];
	(*this)(verts, color);
}

void LineBuffer::reserve(int num_lines, int num_elem) {
	ElementBuffer::reserve(num_lines * 2, num_elem);
}

vector<DrawCall> LineBuffer::drawCalls(bool compute_boxes) const {
	return ElementBuffer::drawCalls(PrimitiveType::lines, compute_boxes);
}
}
