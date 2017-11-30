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

void TriangleBuffer::operator()(const FBox &box, IColor color) {
	auto corners = box.corners();
	int indices[12][3] = {{0, 2, 3}, {0, 3, 1}, {1, 3, 7}, {1, 7, 5}, {2, 6, 7}, {2, 7, 3},
						  {0, 6, 2}, {0, 4, 6}, {0, 5, 4}, {0, 1, 5}, {4, 7, 6}, {4, 5, 7}};

	float3 points[36];
	for(int n = 0; n < 12; n++)
		for(int i = 0; i < 3; i++)
			points[n * 3 + i] = corners[indices[n][i]];
	insertBack(m_positions, points);
	m_colors.resize(m_colors.size() + 36, color);
}

void TriangleBuffer::operator()(const FBox &box, const Matrix4 &matrix, IColor color) {
	(*this)(box, color);
	for(auto &vec : Span<float3>(m_positions.end() - 36, m_positions.end()))
		vec = mulPoint(matrix, vec);
}

void TriangleBuffer::operator()(CSpan<FBox> boxes, IColor color) {
	for(auto &box : boxes)
		(*this)(box, color);
}

void TriangleBuffer::reserve(int num_tris, int num_elem) {
	ElementBuffer::reserve(num_tris * 3, num_elem);
}
}
