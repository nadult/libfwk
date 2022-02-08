// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/math_base.h"
#include "fwk/tag_id.h"

namespace fwk {

// There are 2 kinds of entities which identify graph elements:
// - indices (VertexId, EdgeId, etc.) which keep only index of given element
// - references (Vertex, Edge, etc.) which also store a pointer to Graph in which given
//   element exists; This is the main interface to iterate, find and introspect graph elements

// -------------------------------------------------------------------------------------------
// ---  Graph elements -----------------------------------------------------------------------

using TriangleId = TagId<Tag::triangle>;
using PolygonId = TagId<Tag::polygon>;
using VertexId = TagId<Tag::vertex>;
using EdgeId = TagId<Tag::edge>;
using CellId = TagId<Tag::cell>;

using VertId = VertexId;
using TriId = TriangleId;
using PolyId = PolygonId;

using VertexIdPair = Pair<VertexId>;

class VertexRef;
class EdgeRef;
class TriangleRef;

DEFINE_ENUM(GLayer, l1, l2, l3, l4, l5, l6, l7, l8);
using GLayers = EnumFlags<GLayer>;

struct GLabel {
	u32 color = 0xffffffff;
	int ival1 = 0;
	int ival2 = 0;
	float fval1 = 0.0f;
	float fval2 = 0.0f;

	FWK_ORDER_BY_DECL(GLabel);
};

template <class Ref, class Id> struct GRefs;

class Graph;
template <class T> class GeomGraph;

// -------------------------------------------------------------------------------------------
// ---  Other declarations -------------------------------------------------------------------

using VertexRefs = GRefs<VertexRef, VertexId>;
using EdgeRefs = GRefs<EdgeRef, EdgeId>;
using TriRefs = GRefs<TriangleRef, TriangleId>;

template <class T> class Contour;
template <class T> class SubContourRef;

class Voronoi;

template <c_vec<2> T, c_integral_vec<2> IT = int2> class RegularGrid;
template <class T> class SegmentGrid;

using Contour2F = Contour<float2>;
using Contour3F = Contour<float3>;

using RegularGrid2F = RegularGrid<float2>;
using RegularGrid2D = RegularGrid<double2>;

using Graph2F = GeomGraph<float2>;
using Graph3F = GeomGraph<float3>;
using Graph2I = GeomGraph<int2>;
using Graph3I = GeomGraph<int3>;

// -------------------------------------------------------------------------------------------
// ---  Geom functions -----------------------------------------------------------------------

// Computes scale value which will fit given box into given resolution.
// Scaled values will be in range: <-resolution, resolution>.
template <class T> double integralScale(const Box<T> &, int resolution = 1024 * 1024 * 16);
template <c_vec T> Box<T> enclose(SparseSpan<T>);

// TODO: similar as in GeomGraph
template <class T> Ex<vector<int2>> toIntegral(CSpan<T>, double scale);

// CCW order starting from zero_vector
template <c_vec<2> T>
void orderByDirection(Span<int> indices, CSpan<T> vectors, const T &zero_vector);

// TODO: better names ?
template <class T, class Ret = decltype(DECLVAL(T).xz())>
PodVector<Ret> flatten(SparseSpan<T> span, Axes2D axes) {
	PodVector<Ret> out(span.spread());
	for(int n : span.indices())
		out[n] = flatten(span[n], axes);
	return out;
}

template <class T> auto flatten(const T &obj, Axes2D axes) {
	if constexpr(dim<T> == 3)
		return axes == Axes2D::xy ? obj.xy() : axes == Axes2D::xz ? obj.xz() : obj.yz();
	else
		return obj;
}

template <c_vec<2> T, class S, class Ret = MakeVec<Scalar<T>, 3>>
Ret addThirdAxis(const T &obj, Axes2D axes, S third) {
	return axes == Axes2D::xy	? Ret(obj[0], obj[1], third)
		   : axes == Axes2D::xz ? Ret(obj[0], third, obj[1])
								: Ret(third, obj[0], obj[1]);
}

Line2<float> approximateLine(CSpan<float2> points);
}
