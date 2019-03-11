// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/line_buffer.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/math/box.h"
#include "fwk/math/segment.h"
#include "fwk/math/triangle.h"
#include "fwk/sys/assert.h"

namespace fwk {

LineBuffer::LineBuffer() { DASSERT(m_flags == Opt::colors); }

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
	insertBack(m_positions, segs.reinterpret<float3>());
	m_colors.reserve(m_colors.size() + colors.size() * 2);
	for(auto col : colors)
		insertBack(m_colors, {col, col});
}

void LineBuffer::operator()(const Segment3F &seg, IColor color) {
	insertBack(m_positions, {seg.from, seg.to});
	m_colors.resize(m_colors.size() + 2, color);
}

void LineBuffer::operator()(CSpan<Segment3F> segs, IColor color) {
	(*this)(segs.reinterpret<float3>(), color);
}

void LineBuffer::operator()(const FBox &bbox, IColor color) {
	auto corners = bbox.corners();
	int indices[] = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 3, 7, 2, 6};
	float3 verts[arraySize(indices)];
	for(int i = 0; i < arraySize(indices); i++)
		verts[i] = corners[indices[i]];
	(*this)(verts, color);
}

void LineBuffer::operator()(const FBox &bbox, const Matrix4 &matrix, IColor color) {
	(*this)(bbox, color);
	for(auto &vec : Span<float3>(m_positions.end() - 24, m_positions.end()))
		vec = mulPoint(matrix, vec);
}

void LineBuffer::operator()(const Triangle3F &tri, IColor color) {
	insertBack(m_positions, {tri[0], tri[1], tri[1], tri[2], tri[2], tri[0]});
	m_colors.resize(m_colors.size() + 6, color);
}

void LineBuffer::operator()(CSpan<FBox> boxes, IColor color) {
	for(auto &box : boxes)
		(*this)(box, color);
}

void LineBuffer::operator()(CSpan<Triangle3F> tris, IColor color) {
	for(auto &tri : tris)
		(*this)(tri, color);
}

void LineBuffer::operator()(const FRect &rect, IColor color) {
	auto corners = rect.corners();
	array<int, 8> indices{{0, 1, 1, 2, 2, 3, 3, 0}};

	insertBack(m_positions, transform(indices, [&](int i) { return float3(corners[i], 0.0f); }));
	m_colors.resize(m_positions.size(), color);
}

void LineBuffer::operator()(const Triangle2F &tri, IColor color) {
	float3 points[6];
	for(int i = 0; i < 3; i++) {
		points[i * 2 + 0] = float3(tri[i], 0.0f);
		points[i * 2 + 1] = float3(tri[(i + 1) % 3], 0.0f);
	}
	insertBack(m_positions, points);
	m_colors.resize(m_positions.size(), color);
}

void LineBuffer::operator()(const IRect &rect, IColor color) { (*this)(FRect(rect), color); }

void LineBuffer::operator()(const Segment2F seg, IColor color) {
	(*this)(Segment3F(float3(seg.from, 0.0f), float3(seg.to, 0.0f)), color);
}

void LineBuffer::operator()(const Segment2I seg, IColor color) { (*this)(Segment2F(seg), color); }

void LineBuffer::operator()(CSpan<float2> pos, CSpan<IColor> colors) {
	DASSERT(pos.size() % 2 == 0);
	DASSERT(!colors || colors.size() == pos.size());

	m_positions.reserve(m_positions.size() + pos.size());
	for(auto p : pos)
		m_positions.emplace_back(p, 0.0f);

	if(colors)
		insertBack(m_colors, colors);
	else
		m_colors.resize(m_positions.size(), ColorId::white);
}

void LineBuffer::operator()(CSpan<float2> pos, IColor color) {
	DASSERT(pos.size() % 2 == 0);

	m_positions.reserve(m_positions.size() + pos.size());
	for(auto p : pos)
		m_positions.emplace_back(p, 0.0f);
	m_colors.resize(m_positions.size(), color);
}

void LineBuffer::reserve(int num_lines, int num_elem) {
	ElementBuffer::reserve(num_lines * 2, num_elem);
}

vector<DrawCall> LineBuffer::drawCalls(bool compute_boxes) const {
	return ElementBuffer::drawCalls(PrimitiveType::lines, compute_boxes);
}
}
