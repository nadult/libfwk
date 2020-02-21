// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom/graph.h"
#include "fwk/geom_base.h"
#include "fwk/hash_map.h"
#include "fwk/vector_map.h"
#include <map>

namespace fwk {

// A graph where each vertex also has a position (2D or 3D)
template <class T> class GeomGraph : public Graph {
  public:
	static_assert(is_vec<T> && (dim<T> == 2 || dim<T> == 3));

	using Point = T;
	using Vec = T;
	using Vec2 = MakeVec<Base<T>, 2>;
	using Segment2 = fwk::Segment<Vec2>;
	using VecD = MakeVec<double, dim<T>>;
	using IPoint = MakeVec<int, dim<T>>;
	using Grid = SegmentGrid<Vec2>;
	using Triangle = fwk::Triangle<Base<T>, dim<T>>;

	using PointMap = If<is_rational<Point>, std::map<Point, int>, HashMap<Point, int>>;

	GeomGraph();
	GeomGraph(vector<Point>);
	GeomGraph(vector<Pair<VertexId>>, vector<Point>);
	GeomGraph(CSpan<Triangle>);
	GeomGraph(Graph, PodVector<Point>, PointMap);
	GeomGraph(const Graph &source, PodVector<Point> new_points, PointMap point_map,
			  CSpan<Pair<VertexId>> collapsed_verts);
	FWK_COPYABLE_CLASS(GeomGraph);

	// -------------------------------------------------------------------------------------------
	// ---  Access to graph elements -------------------------------------------------------------

	SparseSpan<Point> points() const;
	vector<Segment<Point>> segments() const;
	Box<Point> boundingBox() const;

	// Low level access
	CSpan<Point> indexedPoints() const { return m_points; }

	using Graph::operator[];

	Point operator()(VertexId id) const { return m_points[id]; }
	Segment<Point> operator()(EdgeId) const;
	Triangle operator()(TriangleId) const;
	Vec vec(EdgeId) const;

	Maybe<VertexRef> findVertex(Point pt) const;

	using Graph::findEdge;
	Maybe<EdgeRef> findEdge(Point p1, Point p2, Layers = all<Layer>) const;

	Maybe<EdgeRef> findFake(VertexId, VertexId) const;
	Maybe<EdgeRef> findFake(Point, Point) const;

	// -------------------------------------------------------------------------------------------
	// ---  Adding & removing elements -----------------------------------------------------------

	VertexId addVertex() = delete;
	void addVertexAt(VertexId, Layers) = delete;
	void addVertexAt(VertexId, const Point &, Layers = Layer::l1);

	FixedElem<VertexId> fixVertex(const Point &, Layers = Layer::l1);
	// Edges & triangles (2 points are enough) between merged points are removed
	// First vertex's index will be used
	void mergeVerts(CSpan<VertexId>, const Point &, Layers = Layer::l1);

	using Graph::addEdge;
	using Graph::addEdgeAt;
	using Graph::fixEdge;
	FixedElem<EdgeId> fixEdge(Point p1, Point p2, Layer = Layer::l1);
	FixedElem<EdgeId> fixEdge(const Segment<Point> &, Layer = Layer::l1);

	using Graph::addTri;
	using Graph::fixTri;

	void remove(VertexId);
	void remove(EdgeId id) { Graph::remove(id); }
	void remove(TriId id) { Graph::remove(id); }

	// Czy warto nazwać te funkcje, removeVertex, removeEdge, removeTri?
	// jeśli mają id-ki to jest to oczywiste, z punktami nie
	// ale z drugiej strony, chcemy, żeby nazewnictwo było spójne ?
	bool removeVertex(const Point &);
	bool removeEdge(const Point &, const Point &);
	bool removeEdge(const Segment<Point> &);

	void reserveVerts(int);
	using Graph::reserveEdges;
	using Graph::reserveTris;

	// -------------------------------------------------------------------------------------------
	// ---  Comparisons and other stuff ----------------------------------------------------------

	int compare(const GeomGraph &) const;
	bool operator==(const GeomGraph &) const;
	bool operator<(const GeomGraph &) const;

	void orderEdges(VertexId, Axes2D = {});

	template <class U = T, EnableIfFptVec<U>...>
	Ex<GeomGraph<IPoint>> toIntegral(double scale) const;
	template <class U = T, EnableIfFptVec<U>...>
	GeomGraph<IPoint> toIntegralWithCollapse(double scale) const;

	template <class U> Ex<GeomGraph<U>> replacePoints(PodVector<U> points) const {
		vector<Pair<VertexId>> collapsed_verts;
		auto point_map = GeomGraph<U>::buildPointMap(vertexValids(), points, collapsed_verts);
		if(collapsed_verts)
			return ERROR("Duplicated points found");
		return GeomGraph<U>(*this, points, point_map);
	}

	// It may still create duplicated edges:
	// if we had V1->V2, V1->V3 after collapsing V2 & V3 we will have two edges V1->V2 (collapsed from V2 & V3)
	template <class U> GeomGraph<U> replacePointsWithCollapse(PodVector<U> points) const {
		vector<Pair<VertexId>> collapsed_verts;
		auto point_map = GeomGraph<U>::buildPointMap(vertexValids(), points, collapsed_verts);
		if(collapsed_verts)
			return GeomGraph<U>(*this, points, point_map, collapsed_verts);
		else
			return GeomGraph<U>(*this, points, point_map);
	}

	static PointMap buildPointMap(CSpan<bool> valid_indices, CSpan<Point> points,
								  vector<Pair<VertexId>> &identical_points);

	// -------------------------------------------------------------------------------------------
	// ---  Grid-based algorithms ----------------------------------------------------------------

	Axes2D m_flat_axes = Axes2D::xz; // TODO: xy ?
	Grid makeGrid() const;

	Vec2 flatPoint(VertexId) const;
	Segment2 flatSegment(EdgeId) const;

	vector<EdgeId> findIntersectors() const;
	bool isPlanar() const;
	Ex<void> checkPlanar() const;

	vector<double2> randomPoints(Random &, double min_distance, Maybe<DRect> = none) const;

	// -------------------------------------------------------------------------------------------
	// ---  Other algorithms ---------------------------------------------------------------------

	struct MergedVerts {
		vector<T> new_points;
		vector<int> num_verts;
		vector<VertexId> indices;
	};

	// Returns list of merged verts (first: what is merged, second: into what)
	// Verts are far enough from others are left alone
	MergedVerts mergeNearby(double merge_dist) const;

	double scale = 1.0;

  private:
	template <Axes2D> void orderEdges(VertexId);

	PodVector<Point> m_points;
	PointMap m_point_map;
};
}
