// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"

namespace fwk {

// Floating point based graphs will be converted to ints to compute Delaunay accurately.
// If conversion cannot be performed, error will be returned (TODO)

// TODO: a co jeśli mamy dziury w wektorze ?
vector<VertexIdPair> delaunay(CSpan<int2>);
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

vector<VertexIdPair> cdtFilterSide(const GeomGraph<int2> &, CSpan<VertexIdPair> cdt, bool ccw_side);

vector<array<int, 3>> delaunaySideTriangles(const GeomGraph<int2> &, CSpan<VertexIdPair> cdt,
											CSpan<VertexIdPair> filter, bool ccw_side);

template <class T, EnableIfFptVec<T, 2>...>
vector<Segment<T>> delaunaySegments(CSpan<VertexIdPair>, const GeomGraph<T> &);

// TODO: this is invalid
template <class T, class TReal = typename T::Scalar, int N = T::vec_size, EnableIfFptVec<T>...>
vector<Triangle<TReal, N>> delaunayTriangles(CSpan<VertexIdPair>, CSpan<T> points);

vector<array<int, 3>> delaunayTriangles(CSpan<VertexIdPair>);
}
