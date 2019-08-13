#include "fwk/geom/geom_graph.h"

#include "fwk/math/segment.h"

namespace fwk {

template <class T> using FixedElem = Graph::FixedElem<T>;

template <class T> GeomGraph<T>::GeomGraph(vector<Point> points) {
	for(auto pt : points)
		fixVertex(pt);
}

template <class T>
GeomGraph<T>::GeomGraph(Graph graph, vector<Point> points, NodeMap node_map)
	: Graph(move(graph)), m_node_map(move(node_map)) {
	m_points.unsafeSwap(points);
}

// -------------------------------------------------------------------------------------------
// ---  Access to graph elements -------------------------------------------------------------

template <class T> vector<T> GeomGraph<T>::points() const {
	return transform(vertexIds(), [&](VertexId id) { return m_points[id]; });
}

template <class T> vector<Segment<T>> GeomGraph<T>::segments() const {
	auto &graph = *this;
	return fwk::transform(edgeIds(), [&](EdgeId id) { return graph(id); });
}

template <class T> Segment<T> GeomGraph<T>::operator()(EdgeId id) const {
	auto [n1, n2] = m_edges[id];
	return {m_points[n1], m_points[n2]};
}

template <class T> Maybe<VertexRef> GeomGraph<T>::findVertex(Point pt) const {
	if(auto it = m_node_map.find(pt); it != m_node_map.end())
		return ref(VertexId(it->second));
	return none;
}

template <class T> Maybe<EdgeRef> GeomGraph<T>::findEdge(Point p1, Point p2, Layers layers) const {
	if(auto n1 = findVertex(p1))
		if(auto n2 = findVertex(p2))
			return findEdge(*n1, *n2, layers);
	return none;
}

// -------------------------------------------------------------------------------------------
// ---  Adding & removing elements -----------------------------------------------------------

template <class T> void GeomGraph<T>::addVertexAt(VertexId vid, const Point &point, Layers layers) {
	DASSERT(m_node_map.find(point) == m_node_map.end());
	Graph::addVertexAt(vid, layers);
	m_points.resize(Graph::m_verts.capacity());
	m_points[vid] = point;
	m_node_map[point] = vid;
}

template <class T> FixedElem<VertexId> GeomGraph<T>::fixVertex(const Point &point, Layers layers) {
	// TODO: możliwośc sprecyzowania domyślnego elementu w hashMap::operator[] ?
	auto it = m_node_map.find(point);
	if(it != m_node_map.end()) {
		m_vert_layers[it->second] |= layers;
		return {VertexId(it->second), false};
	}
	auto id = Graph::addVertex(layers);
	m_points.resize(Graph::m_verts.capacity());
	m_points[id] = point;
	m_node_map[point] = id;
	return {id, true};
}

template <class T> FixedElem<GEdgeId> GeomGraph<T>::fixEdge(Point p1, Point p2, Layer layer) {
	auto n1 = fixVertex(p1).id;
	auto n2 = fixVertex(p2).id;
	return fixEdge(n1, n2, layer);
}

template <class T>
FixedElem<GEdgeId> GeomGraph<T>::fixEdge(const Segment<Point> &seg, Layer layer) {
	return fixEdge(seg.from, seg.to, layer);
}

template <class T> void GeomGraph<T>::remove(VertexId id) {
	m_node_map.erase(m_points[id]);
	Graph::remove(id);
}

template <class T> bool GeomGraph<T>::removeVertex(const Point &pt) {
	if(auto nid = findVertex(pt)) {
		remove(*nid);
		return true;
	}
	return false;
}

template <class T> bool GeomGraph<T>::removeEdge(const Point &from, const Point &to) {
	if(auto eid = findEdge(from, to)) {
		remove(*eid);
		return true;
	}
	return false;
}

template <class T> bool GeomGraph<T>::removeEdge(const Segment<Point> &seg) {
	return removeEdge(seg.from, seg.to);
}

template <class T> void GeomGraph<T>::reserveVerts(int capacity) {
	Graph::reserveVerts(capacity);
	m_points.resize(m_verts.capacity());
}

// -------------------------------------------------------------------------------------------
// ---  Comparisons and other stuff ----------------------------------------------------------

template <class T> int GeomGraph<T>::compare(const GeomGraph &rhs) const {
	// TODO: test it
	if(auto cmp = Graph::compare(rhs))
		return cmp;
	FATAL("writeme");
	//if(auto cmp = m_points.compare(rhs.m_points))
	//	return cmp;
	return 0;
}

template <class T> bool GeomGraph<T>::operator==(const GeomGraph &rhs) const {
	return numVerts() == rhs.numVerts() && numEdges() == rhs.numEdges() && compare(rhs) == 0;
}

template <class T> bool GeomGraph<T>::operator<(const GeomGraph &rhs) const {
	return compare(rhs) == -1;
}

template class GeomGraph<float2>;
template class GeomGraph<int2>;
template class GeomGraph<double2>;

template class GeomGraph<float3>;
template class GeomGraph<int3>;
template class GeomGraph<double3>;
}
