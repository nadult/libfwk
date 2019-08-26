// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"

namespace fwk {

// Floating point based graphs will be converted to ints to compute Delaunay accurately.
// If conversion cannot be performed, error will be returned.

// Positive convex means additionally that no two adjacent edges are parallel to each other
bool isPositiveConvexQuad(CSpan<int2, 4> corners);
bool insideCircumcircle(const int2 &p1, const int2 &p2, const int2 &p3, const int2 &p);

int selectCCWMaxAngle(int2 vec1, CSpan<int2> vecs);
int selectCWMaxAngle(int2 vec1, CSpan<int2> vecs);
llint polygonArea(CSpan<int2> points);

static constexpr int delaunay_integral_resolution = 512 * 1024 * 1024;
template <class T> double delaunayIntegralScale(CSpan<T>);

vector<VertexIdPair> delaunay(CSpan<int2>);
vector<VertexIdPair> delaunay(SparseSpan<int2>);
template <class T, EnableIfVec<T, 2>...> Ex<vector<VertexIdPair>> delaunay(CSpan<T>);

// Warning: you have to be careful with this function:
// VoronoiDiagram removes degenerate edges, which may cause
// delaunay triangulation to miss some triangles in the end
vector<VertexIdPair> delaunay(const VoronoiDiagram &voronoi);

vector<VertexIdPair> constrainedDelaunay(const GeomGraph<int2> &,
										 CSpan<VertexIdPair> delaunay = {});
template <class T, EnableIfVec<T, 2>...>
Ex<vector<VertexIdPair>> constrainedDelaunay(const GeomGraph<T> &,
											 CSpan<VertexIdPair> delaunay = {});

// Each loop has to have at least 3 verts
template <class T> bool isForestOfLoops(const GeomGraph<T> &);

// Returns edges on CCW or CW side, looking from given set of edges
vector<VertexIdPair> cdtFilterSide(CSpan<int2>, CSpan<VertexIdPair> cdt, bool ccw_side);
vector<VertexIdPair> cdtFilterSide(const GeomGraph<int2> &, CSpan<VertexIdPair> cdt, bool ccw_side);

vector<array<int, 3>> delaunayTriangles(const GeomGraph<int2> &, CSpan<VertexIdPair> cdt,
										CSpan<VertexIdPair> filter, bool ccw_side,
										bool invert_filter);

template <class T, EnableIfFptVec<T, 2>...>
vector<Segment<T>> delaunaySegments(CSpan<VertexIdPair>, const GeomGraph<T> &);
}
