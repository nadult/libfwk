// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
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

class VertexRef;
class EdgeRef;
class TriangleRef;

DEFINE_ENUM(GraphLayer, l1, l2, l3, l4, l5, l6, l7, l8);
using GraphLayers = EnumFlags<GraphLayer>;

struct GraphLabel {
	u32 color = 0xffffffff;
	int ival1 = 0;
	int ival2 = 0;
	float fval1 = 0.0f;
	float fval2 = 0.0f;

	FWK_ORDER_BY_DECL(GraphLabel);
};

template <class Ref, class Id, bool pooled> struct GraphRefs;

using PooledVertexRefs = GraphRefs<VertexRef, VertexId, true>;
using PooledEdgeRefs = GraphRefs<EdgeRef, EdgeId, true>;
using PooledTriRefs = GraphRefs<TriangleRef, TriangleId, true>;

using VertexRefs = GraphRefs<VertexRef, VertexId, false>;
using EdgeRefs = GraphRefs<EdgeRef, EdgeId, false>;
using TriRefs = GraphRefs<TriangleRef, TriangleId, false>;

using VertId = VertexId;
using TriId = TriangleId;
using PolyId = PolygonId;

template <class T> class Contour;
template <class T> class SubContourRef;

class VoronoiDiagram;

template <class T, class IT = int2> class RegularGrid;
template <class T> class SegmentGrid;

using Contour2F = fwk::Contour<float2>;
using Contour3F = fwk::Contour<float3>;

using RegularGrid2F = RegularGrid<float2>;
using RegularGrid2D = RegularGrid<double2>;

template <class Vec2> bool onTheEdge(const Box<Vec2> &rect, const Vec2 &p) {
	return isOneOf(p.x, rect.x(), rect.ex()) || isOneOf(p.y, rect.y(), rect.ey());
}

class Graph;
template <class T> class GeomGraph;

using Graph2F = GeomGraph<float2>;
using Graph3F = GeomGraph<float3>;
using Graph2I = GeomGraph<int2>;
using Graph3I = GeomGraph<int3>;

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
