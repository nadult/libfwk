// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/line_buffer.h"
#include "fwk/gfx/triangle_buffer.h"

#include "fwk/geom_base.h"
#include "fwk/gfx/colored_quad.h"
#include "fwk/gfx/colored_triangle.h"
#include "fwk/gfx/drawing.h"
#include "fwk/gfx/visualizer3.h"
#include "fwk/math/matrix4.h"
#include "fwk/vector.h"

namespace fwk {

Visualizer3::Visualizer3(float point_scale) : m_point_scale(point_scale) {}
FWK_COPYABLE_CLASS_IMPL(Visualizer3);

bool Visualizer3::isFullyTransparent(IColor color) const {
	return color.a == 0; // TODO: check material color
}

void Visualizer3::clear() {
	m_lines.clear();
	m_tris.clear();
}

vector<SimpleDrawCall> Visualizer3::drawCalls(bool compute_boxes) const {
	FATAL("writeme please");
	return {};
	/*auto out = m_tris.drawCalls(compute_boxes);
	insertBack(out, m_lines.drawCalls(compute_boxes));
	return out;*/
}

void Visualizer3::setTrans(Matrix4 mat, ModeFlags flags) {
	if(flags & Mode::solid)
		m_tris.setTrans(mat);
	if(flags & Mode::wireframe)
		m_lines.setTrans(mat);
}

void Visualizer3::setMaterial(SimpleMaterial mat, ModeFlags flags) {
	// TODO: what if we're rendering lines in solid mode ?
	if(flags & Mode::solid)
		m_tris.setMaterial(mat);
	if(flags & Mode::wireframe)
		m_lines.setMaterial(mat);
}

void Visualizer3::setMode(VisMode mode) { m_mode = mode; }

void Visualizer3::operator()(const Triangle3F &tri, IColor color) {
	if(isFullyTransparent(color))
		return;

	if(m_mode == Mode::solid)
		m_tris(tri, color);
	else
		m_lines(tri, color);
}

void Visualizer3::operator()(const FBox &box, IColor color) {
	if(isFullyTransparent(color))
		return;

	if(m_mode == Mode::solid)
		m_tris(box, color);
	else
		m_lines(box, color);
}

void Visualizer3::operator()(const Segment3F &seg, IColor color) {
	if(!isFullyTransparent(color))
		m_lines(seg, color);
}

void Visualizer3::operator()(CSpan<Triangle3F> span, IColor color) {
	if(isFullyTransparent(color))
		return;

	if(m_mode == Mode::solid)
		m_tris(span, color);
	else
		m_lines(span, color);
}

void Visualizer3::operator()(CSpan<FBox> span, IColor color) {
	if(isFullyTransparent(color))
		return;

	if(m_mode == Mode::solid)
		m_tris(span, color);
	else
		m_lines(span, color);
}

void Visualizer3::operator()(CSpan<Segment3F> span, IColor color) {
	if(!isFullyTransparent(color))
		m_lines(span, color);
}

void Visualizer3::operator()(const ColoredTriangle &tri) {
	if(!isFullyTransparent(tri.colors[0]))
		(*this)(tri, tri.colors[0]);
}

void Visualizer3::operator()(const ColoredQuad &quad) {
	if(isFullyTransparent(quad.color))
		return;

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
