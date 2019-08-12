// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/line_buffer.h"
#include "fwk/gfx/triangle_buffer.h"

#include "fwk/geom_base.h"
#include "fwk/gfx/colored_quad.h"
#include "fwk/gfx/colored_triangle.h"
#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/visualizer3.h"
#include "fwk/math/matrix4.h"
#include "fwk/vector.h"

namespace fwk {

Visualizer3::Visualizer3(float point_scale) : m_point_scale(point_scale) {}
FWK_COPYABLE_CLASS_IMPL(Visualizer3);

void Visualizer3::clear() {
	m_lines.clear();
	m_tris.clear();
}

vector<DrawCall> Visualizer3::drawCalls(bool compute_boxes) const {
	auto out = m_tris.drawCalls(compute_boxes);
	insertBack(out, m_lines.drawCalls(compute_boxes));
	return out;
}

void Visualizer3::setTrans(Matrix4 mat, ModeFlags flags) {
	if(flags & Mode::solid)
		m_tris.setTrans(mat);
	if(flags & Mode::wireframe)
		m_lines.setTrans(mat);
}

void Visualizer3::setMaterial(Material mat, ModeFlags flags) {
	if(flags & Mode::solid)
		m_tris.setMaterial(mat);
	if(flags & Mode::wireframe)
		m_lines.setMaterial(mat);
}

void Visualizer3::setMode(VisMode mode) { m_mode = mode; }

void Visualizer3::operator()(const Triangle3F &tri, IColor color) {
	if(m_mode == Mode::solid)
		m_tris(tri, color);
	else
		m_lines(tri, color);
}

void Visualizer3::operator()(const FBox &box, IColor color) {
	if(m_mode == Mode::solid)
		m_tris(box, color);
	else
		m_lines(box, color);
}

void Visualizer3::operator()(const Segment3F &seg, IColor color) { m_lines(seg, color); }

void Visualizer3::operator()(CSpan<Triangle3F> span, IColor color) {
	if(m_mode == Mode::solid)
		m_tris(span, color);
	else
		m_lines(span, color);
}

void Visualizer3::operator()(CSpan<FBox> span, IColor color) {
	if(m_mode == Mode::solid)
		m_tris(span, color);
	else
		m_lines(span, color);
}

void Visualizer3::operator()(CSpan<Segment3F> span, IColor color) { m_lines(span, color); }

void Visualizer3::operator()(const ColoredTriangle &tri) { (*this)(tri, tri.colors[0]); }

void Visualizer3::operator()(const ColoredQuad &quad) {
	if(m_mode == Mode::solid) {
		m_tris({Triangle3F(quad[0], quad[1], quad[2]), Triangle3F(quad[0], quad[2], quad[3])},
			   quad.color);
	} else {
		float3 points[8] = {quad[0], quad[1], quad[1], quad[2], quad[2], quad[3], quad[3], quad[0]};
		m_lines(points, quad.color);
	}
}

float Visualizer3::pointScale() const { return m_point_scale; }
}
