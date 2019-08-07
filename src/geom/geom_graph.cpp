#include "fwk/geom/geom_graph.h"

#include "fwk/math/segment.h"

namespace fwk {

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

// -------------------------------------------------------------------------------------------
// ---  Adding & removing elements -----------------------------------------------------------

template <class T> VertexId GeomGraph<T>::add(const Point &point) {
	if constexpr(is_same<Point, None>)
		return addVertex();
	else {
		auto id = addVertex(m_points);
		m_points[id] = point;
		m_node_map[point] = id;
		return id;
	}
}

template <class T> void GeomGraph<T>::remove(VertexId id) {
	m_node_map.erase(m_points[id]);
	Graph::remove(id);
}

template <class T> void GeomGraph<T>::remove(const Point &pt) {
	auto nid = find(pt);
	DASSERT(nid);
	remove(*nid);
}

template <class T> void GeomGraph<T>::remove(const Point &from, const Point &to) {
	if(auto eid = find(from, to))
		remove(*eid);
}

template <class T> void GeomGraph<T>::remove(const Segment<Point> &seg) {
	remove(seg.from, seg.to);
}

template <class T> void GeomGraph<T>::reserveVerts(int capacity) {
	Graph::reserveVerts(capacity);
	capacity = m_verts.capacity();
	if(m_points.size() < capacity)
		m_points.resizeFullCopy(capacity);
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
	return numNodes() == rhs.numNodes() && numEdges() == rhs.numEdges() && compare(rhs) == 0;
}

template <class T> bool GeomGraph<T>::operator<(const GeomGraph &rhs) const {
	return compare(rhs) == -1;
}

template class GeomGraph<float2>;
template class GeomGraph<int2>;

/*
#define TEMPLATE template <class T>
#define TGRAPH Graph<T>

TEMPLATE TGRAPH::~Graph() = default;

TEMPLATE TGRAPH::Graph(vector<T> tpoints) : Graph(tpoints.size()), m_points(move(tpoints)) {
	if constexpr(!is_rational)
		m_node_map.reserve(m_points.size() * 2);

	for(auto id : indexRange<NodeId>(m_points)) {
		auto it = m_node_map.find(m_points[id]);
		if(it == m_node_map.end())
			m_node_map.emplace(m_points[id], id);
	}
}

TEMPLATE pair<NodeId, bool> TGRAPH::add(const T &point) {
	auto ret = m_node_map.emplace(point, m_points.size());
	if(ret.second)
		m_points.emplace_back(point);
	return make_pair(NodeId(ret.first->second), ret.second);
}

TEMPLATE void TGRAPH::reservePoints(int count) {
	m_points.reserve(count);
	if constexpr(!is_rational)
		m_node_map.reserve(count * 2);
}

TEMPLATE void TGRAPH::reserveEdges(int count) {
	// TODO: writeme
}

TEMPLATE void TGRAPH::clear() {
	Graph::clear();
	m_points.clear();
	m_node_map.clear();
}

TEMPLATE Maybe<NodeId> TGRAPH::find(const T &point) const {
	auto it = m_node_map.find(point);
	return it == m_node_map.end() ? Maybe<NodeId>() : NodeId(it->second);
}

TEMPLATE Maybe<GEdgeId> TGRAPH::find(const T &from, const T &to) const {
	if(auto n1 = find(from))
		if(auto n2 = find(to)) {
			auto ret = find(*n1, *n2);
			return ret ? ret->id() : Maybe<GEdgeId>();
		}
	return none;
}

TEMPLATE VectorMap<NodeId, NodeId> TGRAPH::mapping(const vector<T> &source_points) const {
	vector<Pair<NodeId>> out;
	out.reserve(source_points.size());
	for(auto id : indexRange<NodeId>(source_points))
		if(auto target = find(source_points[id]))
			out.emplace_back(id, *target);
	return out;
}*/

}
