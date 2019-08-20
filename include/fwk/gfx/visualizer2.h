// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/fwd_member.h"
#include "fwk/geom_base.h"
#include "fwk/gfx/color.h"
#include "fwk/math/box.h"
#include "fwk/vector.h"

namespace fwk {

class Visualizer2;

DEFINE_ENUM(VisOpt, cross, dashed /*TODO*/, arrow, solid);
using VisFlags = EnumFlags<VisOpt>;

struct VisStyle {
	VisStyle(IColor color = ColorId::white, VisFlags flags = {}, int pixel_offset = 0)
		: color(color), flags(flags), pixel_offset(pixel_offset) {}
	VisStyle(ColorId color, VisFlags flags = {}, int pixel_offset = 0)
		: VisStyle(IColor(color), flags, pixel_offset) {}

	IColor color;
	VisFlags flags;
	i8 pixel_offset;
};

// - Wizualizacja intów
// -  możliwość przechowywania listy obiektów (zeby nie trzeba bylo ich w glupi sposob przekazywac
//    w investigatorze)
// - skalowanie ?

struct Vis2Label {
	string text;
	FRect rect;
	VisStyle style;
};

struct VoronoiVis2Colors {
	IColor point;
	IColor line, inner_line;
	IColor cell;
	IColor selection;
};

class VoronoiVis2 {
  public:
	VoronoiVis2(Visualizer2 &, const VoronoiDiagram &, VoronoiVis2Colors, Maybe<CellId> sel);

	void drawSegment(GEdgeId, bool draw_sel);
	void drawArc(GEdgeId, bool draw_sel);
	void draw();

  private:
	Visualizer2 &m_vis;
	const VoronoiDiagram &m_diag;
	const GeomGraph<double2> &m_graph;
	VoronoiVis2Colors m_colors;
	Maybe<CellId> m_sel;
};

class Visualizer2 {
  public:
	Visualizer2(float point_scale = 1.0f, float cross_scale = 1.0f);
	FWK_COPYABLE_CLASS(Visualizer2);

	// TODO: interface similar to Visualizer3
	// TODO: merge Vis2 & Vis3 ?
	//
	// TODO: drawing order is important for Vis2; for example:
	// first filled quad, then some lines, then quads again

	void clear();
	vector<pair<FBox, Matrix4>> drawBoxes() const;
	vector<DrawCall> drawCalls(bool compute_boxes = false) const;

	void drawPoint(float2, IColor);
	void drawLine(float2, float2, IColor);
	void drawArrow(float2, float2, IColor, bool solid = false);
	void drawRect(FRect, IColor, bool solid = false);
	void drawTriangle(const Triangle2F &, IColor, bool solid = false);
	void drawCross(float2, IColor);
	void drawVoronoi(const VoronoiDiagram &, VoronoiVis2Colors, Maybe<CellId> selection = none);

	template <class T, EnableIfVec<T, 2>...> void operator()(const T &pt, VisStyle style = {}) {
		if(style.flags & VisOpt::cross)
			drawCross(pt, style.color);
		else
			drawPoint(pt, style.color);
	}

	template <class T, EnableIf<dim<T> == 2>...>
	void operator()(const Segment<T> &seg, VisStyle style = {}) {
		float2 off(m_point_scale * style.pixel_offset, m_point_scale * style.pixel_offset);
		if(style.flags & VisOpt::arrow)
			drawArrow(float2(seg.from) + off, float2(seg.to) - off, style.color,
					  style.flags & VisOpt::solid);
		else
			drawLine(float2(seg.from) + off, float2(seg.to) - off, style.color);
	}

	template <class TRange, EnableIfRange<TRange>...>
	void operator()(const TRange &range, VisStyle style = {}) {
		for(auto elem : range)
			(*this)(elem, style);
	}

	template <class T> void drawGrid(const RegularGrid<T> &, IColor);

	template <class T> void drawPoint(T point, IColor col) { drawPoint(float2(point), col); }
	template <class T> void drawLine(T from, T to, IColor col) {
		drawLine(float2(from), float2(to), col);
	}

	template <class T> void drawRect(Box<T> box, IColor col, bool solid = false) {
		drawRect(FRect(box), col, solid);
	}

	template <class T> void drawTriangle(Triangle<T, 2> tri, IColor col, bool solid = false) {
		drawTriangle(Triangle2F(tri), col, solid);
	}

	template <class T> void drawCross(T pos, IColor col) { drawCross(float2(pos), col); }

	template <class T> void drawContour(CSpan<T>, IColor);
	template <class T> void drawContourPoint(const Contour<T> &, float pos, IColor);

	template <class T>
	void drawVoronoiArea(const vector<vector<array<T, 3>>> &vtris, float alpha = 1.0f);

	//template <class T>
	//void drawPlaneGraph(const PGraph<T> &, IColor line_color, IColor point_color);

	template <class T> void drawContours(CSpan<Contour<T>>, IColor);
	template <class T>
	void drawContourWithVectors(const Contour<T> &, IColor, IColor, float dist = 0.5);
	template <class T> void drawContourLoops(vector<Contour<T>>);

	template <class T> void drawLabel(T pos, Str, VisStyle = {});
	template <class T> void drawLabel(Box<T> box, Str, VisStyle = {});
	CSpan<Vis2Label> labels() const { return m_labels; }

	float pointScale() const { return m_point_scale; }

	LineBuffer &lineBuffer() { return reinterpret_cast<LineBuffer &>(m_lines); }
	TriangleBuffer &triangleBuffer() { return reinterpret_cast<TriangleBuffer &>(m_tris); }

  private:
	template <class T> void instantiator();

	FwdMember<LineBuffer> m_lines;
	FwdMember<TriangleBuffer> m_tris;
	vector<Vis2Label> m_labels;
	float m_point_scale, m_cross_scale;
};
}
