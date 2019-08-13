#pragma once

#include "fwk/geom/graph.h"
#include "fwk/geom_base.h"
#include "fwk/hash_map.h"
#include "fwk/vector_map.h"
#include <map>

namespace fwk {

// A graph where each vertex also has a position (2D or 3D)
template <class TPoint> class GeomGraph : public Graph {
  public:
	static_assert(is_vec<TPoint> && (dim<TPoint> == 2 || dim<TPoint> == 3));

	using EdgeId = GEdgeId;
	using Point = TPoint;
	using Label = GraphLabel;

	using NodeMap = If<is_rational<Point>, std::map<Point, int>, HashMap<Point, int>>;

	GeomGraph() {}
	GeomGraph(vector<Point>);
	GeomGraph(vector<Pair<VertexId>>, vector<Point>);
	GeomGraph(Graph, vector<Point>, NodeMap);

	// -------------------------------------------------------------------------------------------
	// ---  Access to graph elements -------------------------------------------------------------

	vector<Point> points() const;
	vector<Segment<Point>> segments() const;

	using Graph::operator[];

	Point operator()(VertexId id) const { return m_points[id]; }
	Segment<Point> operator()(EdgeId id) const;

	Maybe<VertexRef> findVertex(Point pt) const;

	using Graph::findEdge;
	Maybe<EdgeRef> findEdge(Point p1, Point p2, Layers layers = Layers::all()) const;

	Maybe<EdgeRef> findFake(VertexId, VertexId) const;
	Maybe<EdgeRef> findFake(Point, Point) const;

	// -------------------------------------------------------------------------------------------
	// ---  Adding & removing elements -----------------------------------------------------------

	//Ex<void> replacePoints(vector<Point>);
	template <class T> Ex<GeomGraph<T>> replacePoints(vector<T>) const;

	VertexId addVertex() = delete;
	void addVertexAt(VertexId, Layers) = delete;
	void addVertexAt(VertexId, const Point &, Layers = Layer::l1);

	FixedElem<VertexId> fixVertex(const Point &, Layers = Layer::l1);

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

  private:
	PodVector<Point> m_points;
	NodeMap m_node_map;
};

template <class T>
template <class TOut>
Ex<GeomGraph<TOut>> GeomGraph<T>::replacePoints(vector<TOut> points) const {
	typename GeomGraph<TOut>::NodeMap node_map;
	node_map.reserve(points.size());

	for(auto vid : vertexIds())
		node_map[points[vid]] = vid;
	if(node_map.size() != numVerts())
		return ERROR("Duplicate points");

	return GeomGraph<TOut>{*this, points, node_map};
}
}
