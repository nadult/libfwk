// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/line_buffer.h"
#include "fwk/gfx/triangle_buffer.h"

#include "fwk/gfx/visualizer2.h"

#include "fwk/geom/contour.h"
#include "fwk/geom/regular_grid.h"
#include "fwk/geom/voronoi.h"
#include "fwk/math/random.h"
#include "fwk/math/rotation.h"
#include "fwk/math/triangle.h"

namespace fwk {

VoronoiVis2::VoronoiVis2(Visualizer2 &vis, const VoronoiDiagram &diag, VoronoiVis2Colors colors,
						 Maybe<CellId> sel)
	: m_vis(vis), m_diag(diag), m_graph(diag.graph), m_colors(colors), m_sel(sel) {}

void VoronoiVis2::drawSegment(EdgeId seg_id, bool draw_sel) {
	auto arc_id = m_diag.arcId(seg_id);
	auto cell_id = m_diag.cellId(arc_id);
	auto seg = m_graph.ref(seg_id);

	auto p1 = m_graph(seg.from()), p2 = m_graph(seg.to());
	if(draw_sel != (cell_id == m_sel))
		return;

	auto color = !m_diag.isArcPrimary(arc_id) ? m_colors.inner_line : m_colors.line;
	if(cell_id == m_sel)
		color = m_colors.selection;
	m_vis.drawLine(p1, p2, color);
}

void VoronoiVis2::drawArc(EdgeId eid, bool draw_sel) {
	auto ref = m_graph.ref(eid);
	auto p1 = m_graph(ref.from()), p2 = m_graph(ref.to());
	auto color = !m_diag.isArcPrimary(eid) ? m_colors.inner_line : m_colors.line;
	bool is_sel = m_diag.cellId(eid) == m_sel;
	if(draw_sel != is_sel)
		return;

	if(is_sel)
		color = m_colors.selection;
	m_vis.drawLine(p1, p2, color);
}

void VoronoiVis2::draw() {
	if(bool draw_segs = true) {
		for(auto eid : m_graph.edges(VoronoiDiagram::seg_layer))
			drawSegment(eid, false);
		for(auto eid : m_graph.edges(VoronoiDiagram::seg_layer))
			drawSegment(eid, true);
	}

	if(bool draw_arcs = false) {
		for(auto eid : m_graph.edges(VoronoiDiagram::arc_layer))
			drawArc(eid, false);
		for(auto eid : m_graph.edges(VoronoiDiagram::arc_layer))
			drawArc(eid, true);
	}

	/* // TODO: fix it
	for(auto &cell : m_diag.cells()) {
		auto &gen = cell.generator;

		IColor color = m_colors.cell;
		if(m_sel && &cell == &m_diag[*m_sel])
			color = m_colors.selection;

		if(cell.generator.isEdge()) {
			auto p1 = m_diag[gen.asEdge().first];
			auto p2 = m_diag[gen.asEdge().second];
			m_vis.drawLine(p1, p2, color);
		} else {
			auto p1 = m_diag[gen.asNode()];
			m_vis.drawCross(p1, color);
		}
	}*/
}

using Opt = ElementBufferOpt;

Visualizer2::Visualizer2(float point_scale, float cross_scale)
	: m_tris(Opt::tex_coords | Opt::colors), m_point_scale(point_scale),
	  m_cross_scale(cross_scale) {}
FWK_COPYABLE_CLASS_IMPL(Visualizer2);

void Visualizer2::clear() {
	m_lines.clear();
	m_tris.clear();
}

vector<pair<FBox, Matrix4>> Visualizer2::drawBoxes() const {
	auto dboxes = m_tris.drawBoxes();
	insertBack(dboxes, m_lines.drawBoxes());
	return dboxes;
}

vector<DrawCall> Visualizer2::drawCalls(bool compute_boxes) const {
	auto out = m_tris.drawCalls(compute_boxes);
	insertBack(out, m_lines.drawCalls(compute_boxes));
	return out;
}

void Visualizer2::drawPoint(float2 pos, IColor color) {
	auto psize = float2(3) * float(m_point_scale);
	auto fpos = float2(pos);
	m_tris(FRect(fpos - psize, fpos + psize), color);
}

void Visualizer2::drawLine(float2 p1, float2 p2, IColor color) {
	m_lines(Segment2F(p1, p2), color);
}

void Visualizer2::drawArrow(float2 p1, float2 p2, IColor color, bool solid) {
	auto arrow_vector = -normalize(p2 - p1) * m_point_scale * 10.0f;
	auto angle = degToRad(30.0f);
	auto vec1 = rotateVector(arrow_vector, angle);
	auto vec2 = rotateVector(arrow_vector, -angle);
	auto arrow_pos = (p2 + p1) * 0.5f;

	m_lines(Segment2F(p1, p2), color);
	if(solid) {
		m_tris(Triangle2F(arrow_pos + vec1, arrow_pos, arrow_pos + vec2), color);
	} else {
		m_lines(Segment2F(arrow_pos, arrow_pos + vec1), color);
		m_lines(Segment2F(arrow_pos, arrow_pos + vec2), color);
	}
}

void Visualizer2::drawRect(FRect rect, IColor color, bool solid) {
	if(solid)
		m_tris(FRect(rect), color);
	else
		m_lines(FRect(rect), color);
}

void Visualizer2::drawTriangle(const Triangle2F &tri, IColor color, bool solid) {
	if(solid)
		m_tris(tri, color);
	else
		m_lines(tri, color);
}

void Visualizer2::drawCross(float2 pos, IColor color) {
	float scale = m_cross_scale;
	float2 fpos(pos);
	m_lines(Segment2F(fpos + float2(2, 2) * scale, fpos - float2(2, 2) * scale), color);
	m_lines(Segment2F(fpos + float2(2, -2) * scale, fpos + float2(-2, 2) * scale), color);
}

template <class T> void Visualizer2::drawContour(CSpan<T> points, IColor color) {
	for(int n : intRange(0, points.size() - 1))
		drawLine(points[n], points[n + 1], color);
}

template <class T>
void Visualizer2::drawContourPoint(const Contour<T> &contour, float pos, IColor color) {
	auto tpos = contour.trackPos(pos);
	float2 point(contour.point(tpos));
	float2 tgt(contour.tangent(tpos));

	drawPoint(point, color);
	m_lines(Segment2F(point, (point) + tgt * 10.0f * m_point_scale), color);
}

template <class T>
void Visualizer2::drawVoronoiArea(const vector<vector<array<T, 3>>> &vvtris, float alpha) {
	Random rand;
	for(auto &vtris : vvtris) {
		FColor color = ColorId::black;
		while(isOneOf(color, ColorId::black, ColorId::transparent, ColorId::gray, ColorId::white))
			color = ColorId(rand() % count<ColorId>);

		color.a = alpha;
		vector<float2> tris;
		for(auto t : vtris)
			insertBack(tris, t);
		m_tris(tris, {}, IColor(color));
	}
}

/*
template <class T>
void Visualizer2::drawPlaneGraph(const PGraph<T> &graph, IColor line_color, IColor point_color) {
	if(point_color != ColorId::transparent)
		for(auto id : graph.vertexRefs())
			drawPoint(graph[id], point_color);

	if(line_color != ColorId::transparent)
		for(auto ref : graph.edgeRefs()) {
			float2 rpoint1(graph[ref.from()]);
			float2 rpoint2(graph[ref.to()]);
			m_lines(Segment2F(rpoint1, rpoint2), line_color);
		}
}*/

template <class T> void Visualizer2::drawContours(CSpan<Contour<T>> contours, IColor color) {
	for(auto &contour : contours)
		for(auto edge : contour.segments())
			m_lines(Segment2F(edge), color);
}

template <class T>
void Visualizer2::drawContourWithVectors(const Contour<T> &contour, IColor color, IColor vcolor,
										 float dist) {
	for(auto edge : contour.segments())
		m_lines(Segment2F(edge), color);

	if(vcolor != ColorId::transparent)
		for(double pos = 0.0; pos < contour.length(); pos += dist) {
			auto tpos = contour.trackPos(pos);
			auto point = contour.point(tpos);
			auto tgt = contour.tangent(tpos);

			drawLine(point, point + tgt * m_point_scale * 10.0, vcolor);
		}
}

template <class T> void Visualizer2::drawContourLoops(vector<Contour<T>> cloops) {
	int loop_id = 0;
	for(auto contour : cloops) {
		int idx = 0;
		for(auto edge : contour.segments()) {
			float3 color(0.5f, 0.5f, 0.5f);
			color[loop_id] = 1.0f;
			float value = idx % 2 == 0 ? 1.0f : 0.6f;
			color *= value;
			idx++;

			T p1 = edge.from;
			T p2 = edge.to;
			T normal = rotateVector(normalize(p2 - p1), pi * 0.5f);
			p1 += normal * 0.03f;
			p2 += normal * 0.03f;
			drawLine(p1, p2, IColor(color));
		}
		loop_id = (loop_id + 1) % 3;
	}
}

template <class T> void Visualizer2::drawGrid(const RegularGrid<T> &grid, IColor color) {
	for(int x = 0; x <= grid.size().x; x++) {
		auto x1 = grid.toWorld(int2{x, 0}), x2 = grid.toWorld(int2{x, grid.size().y});
		drawLine(x1, x2, color);
	}
	for(int y = 0; y <= grid.size().y; y++) {
		auto y1 = grid.toWorld(int2{0, y}), y2 = grid.toWorld(int2{grid.size().x, y});
		drawLine(y1, y2, color);
	}
}

template <class T> void Visualizer2::drawLabel(T pos, Str text, VisStyle style) {
	drawLabel(Box<T>(pos, pos), text, style);
}

template <class T> void Visualizer2::drawLabel(Box<T> box, Str text, VisStyle style) {
	m_labels.emplace_back(text, FRect(box), style);
}

#define INSTANTIATE(T)                                                                             \
	template void Visualizer2::drawLabel(T, Str, VisStyle);                                        \
	template void Visualizer2::drawLabel(Box<T>, Str, VisStyle);                                   \
	template void Visualizer2::drawContour(CSpan<T>, IColor);                                      \
	template void Visualizer2::drawGrid(const RegularGrid<T> &, IColor);                           \
	template void Visualizer2::drawVoronoiArea(const vector<vector<array<T, 3>>> &, float);

#define INSTANTIATE_FPT(T)                                                                         \
	INSTANTIATE(T)                                                                                 \
	template void Visualizer2::drawContours(CSpan<Contour<T>>, IColor);                            \
	template void Visualizer2::drawContourPoint(const Contour<T> &, float, IColor);                \
	template void Visualizer2::drawContourWithVectors(const Contour<T> &, IColor, IColor, float);  \
	template void Visualizer2::drawContourLoops(vector<Contour<T>>);

INSTANTIATE(short2)
INSTANTIATE(int2)
INSTANTIATE_FPT(float2)
INSTANTIATE_FPT(double2)
}
