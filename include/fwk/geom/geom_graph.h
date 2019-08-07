#pragma once

#include "fwk/geom/graph.h"
#include "fwk/geom_base.h"
#include "fwk/hash_map.h"
#include "fwk/vector_map.h"
#include <map>

namespace fwk {

// A graph where each vertex also has a position (2D or 3D)
template <class TPoint = None> class GeomGraph : public Graph {
  public:
	using EdgeId = GEdgeId;
	using Point = TPoint;
	using Label = GraphLabel;

	GeomGraph() {}
	GeomGraph(vector<Point>);
	GeomGraph(vector<Pair<VertexId>>, vector<Point>);

	// -------------------------------------------------------------------------------------------
	// ---  Access to graph elements -------------------------------------------------------------

	vector<Point> points() const;
	vector<Segment<Point>> segments() const;

	using Graph::operator[];

	Point operator()(VertexId id) const { return m_points[id]; }
	Segment<Point> operator()(EdgeId id) const;

	Maybe<VertexRef> find(Point pt) const {
		if(auto it = m_node_map.find(pt); it != m_node_map.end())
			return ref(VertexId(it->second));
		return none;
	}

	Maybe<EdgeRef> find(Point p1, Point p2) const {
		if(auto n1 = find(p1))
			if(auto n2 = find(p2))
				return find(*n1, *n2);
		return none;
	}

	Maybe<EdgeRef> find(VertexId n1, VertexId n2) const { return Graph::find(n1, n2); }

	Maybe<EdgeRef> findFake(VertexId, VertexId) const;
	Maybe<EdgeRef> findFake(Point, Point) const;

	// -------------------------------------------------------------------------------------------
	// ---  Adding & removing elements -----------------------------------------------------------

	VertexId add(const Point &);
	pair<EdgeId, bool> add(VertexId v1, VertexId v2) { return Graph::add(v1, v2); }
	pair<TriId, bool> add(VertexId v1, VertexId v2, VertexId v3) { return Graph::add(v1, v2, v3); }
	using Graph::addNew;

	// TODO: make these consistent; adding both verts & edges; what if they exist already ?
	// Should we allow multiple nodes ?
	EdgeId add(Point p1, Point p2) {
		auto n1 = find(p1);
		auto n2 = find(p2);
		DASSERT(n1 && n2);
		return add(*n1, *n2).first;
	}

	// Adds both points and edges; what about node labels ?
	EdgeId add(const Segment<Point> &seg) {
		auto n1 = add(seg.from);
		auto n2 = add(seg.to);
		return add(n1, n2).first;
	}

	void remove(VertexId);
	void remove(EdgeId id) { Graph::remove(id); }
	void remove(TriId id) { Graph::remove(id); }

	void remove(const Point &);
	// TODO: should we remove verts as well ?
	void remove(const Point &, const Point &);
	void remove(const Segment<Point> &);

	void reserveVerts(int);
	using Graph::reserveEdges;
	using Graph::reserveTris;

	// -------------------------------------------------------------------------------------------
	// ---  Comparisons and other stuff ----------------------------------------------------------

	int compare(const GeomGraph &) const;
	bool operator==(const GeomGraph &) const;
	bool operator<(const GeomGraph &) const;

  private:
	using NodeMap = If<is_rational<Point>, std::map<Point, int>, HashMap<Point, int>>;

	PodVector<Point> m_points;
	NodeMap m_node_map;
};
}
