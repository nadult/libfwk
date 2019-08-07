// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"
#include "fwk/tag_id.h"

namespace fwk {

DEFINE_ENUM(GeomTag, vertex, edge, arc, arc_segment, cell, polygon, triangle);

using TriangleId = TagId<GeomTag::triangle>;
using PolygonId = TagId<GeomTag::polygon>;
using VertexId = TagId<GeomTag::vertex>;
using EdgeId = TagId<GeomTag::edge>;
using ArcId = TagId<GeomTag::arc>;
using ArcSegmentId = TagId<GeomTag::arc_segment>;
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
}
