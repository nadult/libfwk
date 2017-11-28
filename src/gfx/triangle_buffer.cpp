// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/triangle_buffer.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/math/box.h"
#include "fwk/math/triangle.h"
#include "fwk/sys/assert.h"

namespace fwk {

vector<DrawCall> TriangleBuffer::drawCalls(bool compute_boxes) const {
	return ElementBuffer::drawCalls(PrimitiveType::triangles, compute_boxes);
}

void TriangleBuffer::operator()(const Triangle3F &tri, IColor color) {
	insertBack(m_positions, tri.points());
	m_colors.resize(m_colors.size() + 3, color);
}

void TriangleBuffer::operator()(CSpan<Triangle3F> tris, IColor color) {
	static_assert(sizeof(Triangle3F) == sizeof(float3) * 3);
	insertBack(m_positions, reinterpret<float3>(tris));
	m_colors.resize(m_colors.size() + tris.size() * 3, color);
}

void TriangleBuffer::operator()(const FBox &box, IColor color) { FATAL("TODO: writeme"); }

void TriangleBuffer::reserve(int num_tris, int num_elem) {
	ElementBuffer::reserve(num_tris * 3, num_elem);
}
}
