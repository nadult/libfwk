// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/triangle_buffer.h"

#include "fwk/gfx/drawing.h"
#include "fwk/math/box.h"
#include "fwk/math/triangle.h"
#include "fwk/sys/assert.h"

namespace fwk {

TriangleBuffer::TriangleBuffer(Flags flags) : ElementBuffer(flags) {}

vector<SimpleDrawCall> TriangleBuffer::drawCalls(bool compute_boxes) const {
	FATAL("writeme");
	return {};
	//return ElementBuffer::drawCalls(VPrimitiveTopology::triangle_list, compute_boxes);
}

void TriangleBuffer::fillBuffers(IColor color) {
	if(m_flags & Opt::colors)
		m_colors.resize(m_positions.size(), color);
	if(m_flags & Opt::tex_coords)
		m_tex_coords.resize(m_positions.size(), float2());
}

void TriangleBuffer::operator()(const Triangle3F &tri, IColor color) {
	insertBack(m_positions, tri.points());
	fillBuffers(color);
}

void TriangleBuffer::operator()(CSpan<Triangle3F> tris, IColor color) {
	static_assert(sizeof(Triangle3F) == sizeof(float3) * 3);
	insertBack(m_positions, tris.reinterpret<float3>());
	fillBuffers(color);
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
	fillBuffers(color);
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

void TriangleBuffer::operator()(const FRect &rect, const FRect &tex_rect, CSpan<IColor, 4> colors) {
	auto pos = rect.corners();

	array<int, 6> indices{{0, 1, 2, 0, 2, 3}};
	insertBack(m_positions, transform(indices, [&](int i) { return float3(pos[i], 0.0f); }));
	if(m_flags & Opt::colors)
		insertBack(m_colors, transform(indices, [&](int i) { return colors[i]; }));
	if(m_flags & Opt::tex_coords) {
		auto uv = tex_rect.corners();
		insertBack(m_tex_coords, transform(indices, [&](int i) { return uv[i]; }));
	}
}

void TriangleBuffer::operator()(const FRect &rect, const FRect &tex_rect, IColor color) {
	(*this)(rect, tex_rect, {color, color, color, color});
}

void TriangleBuffer::operator()(const Triangle2F &tri, IColor color) {
	m_positions.reserve(m_positions.size() + 3);
	for(auto p : tri.points())
		m_positions.emplace_back(p, 0.0f);
	m_colors.resize(m_positions.size(), color);
	m_tex_coords.resize(m_positions.size(), float2());
}

void TriangleBuffer::operator()(const FRect &rect, IColor color) {
	(*this)(rect, FRect({1, 1}), color);
}

void TriangleBuffer::operator()(const IRect &rect, IColor color) {
	(*this)(FRect(rect), FRect({1, 1}), color);
}

void TriangleBuffer::quads(CSpan<float2> pos, CSpan<float2> tex_coords, CSpan<IColor> colors) {
	DASSERT(pos.size() % 4 == 0);
	DASSERT(!tex_coords || tex_coords.size() == pos.size());
	DASSERT(!colors || colors.size() == pos.size());

	array<int, 6> indices{{0, 1, 2, 0, 2, 3}};

	for(int n : intRange(pos.size() / 4)) {
		insertBack(m_positions,
				   transform(indices, [&](int i) { return float3(pos[n * 4 + i], 0.0f); }));

		if((m_flags & Opt::colors) && colors)
			insertBack(m_colors, transform(indices, [&](int i) { return colors[n * 4 + i]; }));
		if((m_flags & Opt::tex_coords) && tex_coords)
			insertBack(m_tex_coords,
					   transform(indices, [&](int i) { return tex_coords[n * 4 + i]; }));
	}

	fillBuffers(ColorId::white);
}

void TriangleBuffer::operator()(CSpan<float2> pos, CSpan<float2> tex_coords, CSpan<IColor> colors) {
	DASSERT(pos.size() % 3 == 0);
	DASSERT(!tex_coords || tex_coords.size() == pos.size());
	DASSERT(!colors || colors.size() == pos.size());

	m_positions.reserve(m_positions.size() + pos.size());
	for(auto p : pos)
		m_positions.emplace_back(p, 0.0f);
	if((m_flags & Opt::colors) && colors)
		insertBack(m_colors, colors);
	if((m_flags & Opt::tex_coords) && tex_coords)
		insertBack(m_tex_coords, tex_coords);
	fillBuffers(ColorId::white);
}

void TriangleBuffer::operator()(CSpan<float2> pos, CSpan<float2> tex_coords, IColor color) {
	DASSERT(pos.size() % 3 == 0);
	DASSERT(!tex_coords || tex_coords.size() == pos.size());

	m_positions.reserve(m_positions.size() + pos.size());
	for(auto p : pos)
		m_positions.emplace_back(p, 0.0f);
	if((m_flags & Opt::tex_coords) && tex_coords)
		insertBack(m_tex_coords, tex_coords);
	fillBuffers(color);
}

void TriangleBuffer::reserve(int num_tris, int num_elem) {
	ElementBuffer::reserve(num_tris * 3, num_elem);
}
}
