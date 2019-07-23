// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"

namespace fwk {

// Floating point based graphs will be converted to ints to compute Delaunay accurately.
// If conversion cannot be performed, error will be returned (TODO)

template <class T, EnableIfVec<T, 2>...> vector<VertexIdPair> delaunay(CSpan<T>);

// Warning: you have to be careful with this function:
// VoronoiDiagram removes degenerate edges, which may cause
// delaunay triangulation to miss some triangles in the end
vector<VertexIdPair> delaunay(const VoronoiDiagram &voronoi);

template <class IT, EnableIfIntegralVec<IT, 2>...>
vector<VertexIdPair> constrainedDelaunay(const PGraph<IT> &, CSpan<VertexIdPair> delaunay = {});

template <class T, EnableIfFptVec<T, 2>...>
vector<VertexIdPair> constrainedDelaunay(const PGraph<T> &, CSpan<VertexIdPair> delaunay = {});

// Each loops has to have at least 3 nodes
template <class T> bool isForestOfLoops(const PGraph<T> &);

template <class T, EnableIfIntegralVec<T, 2>...>
vector<VertexIdPair> cdtFilterSide(const PGraph<T> &, CSpan<VertexIdPair> cdt, bool ccw_side);

template <class T, EnableIfIntegralVec<T, 2>...>
vector<array<int, 3>> delaunaySideTriangles(const PGraph<T> &, CSpan<VertexIdPair> cdt,
											CSpan<VertexIdPair> filter, bool ccw_side);

template <class T, EnableIfFptVec<T, 2>...>
vector<Segment<T>> delaunaySegments(CSpan<VertexIdPair>, const PGraph<T> &);

// TODO: this is invalid
template <class T, class TReal = typename T::Scalar, int N = T::vec_size, EnableIfFptVec<T>...>
vector<Triangle<TReal, N>> delaunayTriangles(CSpan<VertexIdPair>, CSpan<T> points);

vector<array<int, 3>> delaunayTriangles(CSpan<VertexIdPair>);
}
