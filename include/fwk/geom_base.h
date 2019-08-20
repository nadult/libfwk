// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"
#include "fwk/tag_id.h"

namespace fwk {

DEFINE_ENUM(GeomTag, vertex, edge, cell, polygon, triangle);

using TriangleId = TagId<GeomTag::triangle>;
using PolygonId = TagId<GeomTag::polygon>;
using VertexId = TagId<GeomTag::vertex>;
using EdgeId = TagId<GeomTag::edge>;
using CellId = TagId<GeomTag::cell>;

using VertexIdPair = Pair<VertexId>;

using VertId = VertexId;
using TriId = TriangleId;
using PolyId = PolygonId;

template <class T> class Contour;
template <class T> class SubContourRef;

class ImmutableGraph;
class VoronoiDiagram;

template <class T, class IT = int2> class RegularGrid;
template <class T> class SegmentGrid;
template <class T> class PlaneGraphBuilder;
template <class T> class PlaneGraph;

template <class T> using PGraph = PlaneGraph<T>;
template <class T> using PGraphBuilder = PlaneGraphBuilder<T>;

using Contour2F = fwk::Contour<float2>;
using Contour3F = fwk::Contour<float3>;

using RegularGrid2F = RegularGrid<float2>;
using RegularGrid2D = RegularGrid<double2>;

template <class Vec2> bool onTheEdge(const Box<Vec2> &rect, const Vec2 &p) {
	return isOneOf(p.x, rect.x(), rect.ex()) || isOneOf(p.y, rect.y(), rect.ey());
}

class Graph;
template <class T> class GeomGraph;
class VertexRef;
class EdgeRef;

using Graph2F = GeomGraph<float2>;
using Graph3F = GeomGraph<float3>;
using Graph2I = GeomGraph<int2>;
using Graph3I = GeomGraph<int3>;

// TODO: remove it
struct GEdgeId;

// Computes scale value which will fit given box into given resolution.
// Scaled values will be in range: <-resolution, resolution>.
template <class T> double integralScale(const Box<T> &, int resolution = 1024 * 1024 * 16);
template <class T> Box<T> encloseSelected(CSpan<T> points, CSpan<bool> valids);

// TODO: similar as in GeomGraph
template <class T> Ex<vector<int2>> toIntegral(CSpan<T>, double scale);

// CCW order starting from zero_vector
template <class T, EnableIfVec<T, 2>...>
void orderByDirection(Span<int> indices, CSpan<T> vectors, const T &zero_vector);

}
