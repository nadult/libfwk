// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/geom_graph.h"

#include "fwk/geom/segment_grid.h"
#include "fwk/math/direction.h"
#include "fwk/math/segment.h"
#include "fwk/sys/assert.h"

namespace fwk {

template <class T> using FixedElem = Graph::FixedElem<T>;

template <class T> GeomGraph<T>::GeomGraph(vector<Point> points) {
	for(auto pt : points)
		fixVertex(pt);
}

template <class T>
GeomGraph<T>::GeomGraph(Graph graph, PodVector<Point> points, PointMap point_map)
	: Graph(move(graph)), m_points(move(points)), m_point_map(move(point_map)) {}

template <class T>
GeomGraph<T>::GeomGraph(const Graph &graph, PodVector<Point> points, PointMap point_map,
						CSpan<Pair<VertexId>> collapsed_verts)
	: m_points(move(points)), m_point_map(move(point_map)) {
	vector<Maybe<VertexId>> collapses(graph.vertsEndIndex());
	for(auto [from, to] : collapsed_verts)
		collapses[from] = to;

	for(auto vert : graph.verts())
		if(!collapses[vert]) {
			Graph::addVertexAt(vert, graph.layers(vert));
			if(graph.hasLabel(vert))
				(*this)[vert] = graph[vert];
		}

	for(auto edge : graph.edges()) {
		auto v1 = edge.from(), v2 = edge.to();
		if(collapses[v1] == v2 || collapses[v2] == v1)
			continue;
		addEdgeAt(edge, v1, v2, edge.layer());
		if(graph.hasLabel(edge))
			(*this)[edge] = graph[edge];
	}

	if(graph.numTris())
		FATAL("handle me please");
}

// -------------------------------------------------------------------------------------------
// ---  Access to graph elements -------------------------------------------------------------

template <class T> vector<T> GeomGraph<T>::points() const {
	return transform(vertexIds(), [&](VertexId id) { return m_points[id]; });
}

template <class T> Box<T> GeomGraph<T>::boundingBox() const {
	return encloseSelected<T>(m_points, vertexValids());
}

template <class T> vector<Segment<T>> GeomGraph<T>::segments() const {
	auto &graph = *this;
	return fwk::transform(edgeIds(), [&](EdgeId id) { return graph(id); });
}

template <class T> Segment<T> GeomGraph<T>::operator()(EdgeId id) const {
	auto [n1, n2] = m_edges[id];
	return {m_points[n1], m_points[n2]};
}

template <class T> T GeomGraph<T>::vec(EdgeId id) const {
	auto [n1, n2] = m_edges[id];
	return m_points[n2] - m_points[n1];
}

template <class T> Maybe<VertexRef> GeomGraph<T>::findVertex(Point pt) const {
	if(auto it = m_point_map.find(pt); it != m_point_map.end())
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
	DASSERT(m_point_map.find(point) == m_point_map.end());
	Graph::addVertexAt(vid, layers);
	m_points.resize(Graph::m_verts.capacity());
	m_points[vid] = point;
	m_point_map[point] = vid;
}

template <class T> FixedElem<VertexId> GeomGraph<T>::fixVertex(const Point &point, Layers layers) {
	// TODO: możliwośc sprecyzowania domyślnego elementu w hashMap::operator[] ?
	auto it = m_point_map.find(point);
	if(it != m_point_map.end()) {
		m_vert_layers[it->second] |= layers;
		return {VertexId(it->second), false};
	}
	auto id = Graph::addVertex(layers);
	m_points.resize(Graph::m_verts.capacity());
	m_points[id] = point;
	m_point_map[point] = id;
	return {id, true};
}

template <class T> FixedElem<EdgeId> GeomGraph<T>::fixEdge(Point p1, Point p2, Layer layer) {
	auto n1 = fixVertex(p1).id;
	auto n2 = fixVertex(p2).id;
	return fixEdge(n1, n2, layer);
}

template <class T> FixedElem<EdgeId> GeomGraph<T>::fixEdge(const Segment<Point> &seg, Layer layer) {
	return fixEdge(seg.from, seg.to, layer);
}

template <class T> void GeomGraph<T>::remove(VertexId id) {
	m_point_map.erase(m_points[id]);
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

// TODO: move it somewhere ?
template <class T, int static_size> struct LocalBuffer {
	Span<T> make(int size) {
		if(size > static_size) {
			dynamic_buffer.resize(size);
			return dynamic_buffer;
		}
		return {static_buffer, size};
	}

  private:
	T static_buffer[static_size];
	PodVector<T> dynamic_buffer;
};

template <class T> template <Axes2D axes> void GeomGraph<T>::orderEdges(VertexId vid) {
	auto &edges = m_verts[vid];

	using Vec2 = MakeVec<Scalar<T>, 2>;

	struct OrderedEdge {
		Vec2 vec;
		int quadrant;
		VertexEdgeId eid = no_init;

		bool operator<(const OrderedEdge &rhs) const {
			if(quadrant != rhs.quadrant)
				return quadrant < rhs.quadrant;
			return ccwSide(vec, rhs.vec);
		}
	};

	LocalBuffer<OrderedEdge, 32> local_buf;
	auto oedges = local_buf.make(edges.size());
	auto current_pos = m_points[vid];
	for(int n : intRange(edges)) {
		auto edge = ref(edges[n]);
		auto full_vec = m_points[edge.other(vid)] - current_pos;
		Vec2 vec;
		if constexpr(dim<T> == 3)
			vec = axes == Axes2D::xy ? full_vec.xy()
									 : axes == Axes2D::xz ? full_vec.xz() : full_vec.yz();
		else
			vec = full_vec;

		auto quadrant = fwk::quadrant(vec);
		oedges[n] = {vec, quadrant, edges[n]};
	}

	std::sort(begin(oedges), end(oedges));
	for(int n : intRange(edges))
		edges[n] = oedges[n].eid;
}

template <class T> void GeomGraph<T>::orderEdges(VertexId vid, Axes2D axes) {
	if constexpr(dim<T> == 3) {
		switch(axes) {
		case Axes2D::xy:
			return orderEdges<Axes2D::xy>(vid);
		case Axes2D::xz:
			return orderEdges<Axes2D::xz>(vid);
		case Axes2D::yz:
			return orderEdges<Axes2D::yz>(vid);
		}
	}
	orderEdges<Axes2D::xy>(vid);
}

template <class T>
auto GeomGraph<T>::buildPointMap(CSpan<bool> valid_indices, CSpan<Point> points,
								 vector<Pair<VertexId>> &identical_points) -> PointMap {
	DASSERT_EQ(valid_indices.size(), points.size());

	PointMap point_map;
	point_map.reserve(points.size());
	for(auto vid : indexRange<VertexId>(valid_indices))
		if(valid_indices[vid]) {
			auto it = point_map.find(points[vid]);
			if(it != point_map.end())
				identical_points.emplace_back(vid, it->second);
			else
				point_map[points[vid]] = vid;
		}

	return point_map;
}

template <class T>
template <class U, EnableIfFptVec<U>...>
auto GeomGraph<T>::toIntegral(double scale) const -> Ex<GeomGraph<IPoint>> {
	PodVector<IPoint> new_points(vertsEndIndex());
	for(auto vert : verts())
		new_points[vert] = IPoint((*this)(vert)*scale);
	auto out = replacePoints(new_points);
	if(out)
		out->scale = 1.0 / scale;
	return out;
}

template <class T>
template <class U, EnableIfFptVec<U>...>
auto GeomGraph<T>::toIntegralWithCollapse(double scale) const -> GeomGraph<IPoint> {
	PodVector<IPoint> new_points(vertsEndIndex());
	for(auto vert : verts())
		new_points[vert] = IPoint((*this)(vert)*scale);
	auto out = replacePointsWithCollapse(new_points);
	out.scale = 1.0 / scale;
	return out;
}

// -------------------------------------------------------------------------------------------
// ---  Algorithms ---------------------------------------------------------------------------

template <class T> auto GeomGraph<T>::makeGrid() const -> Grid {
	// TODO: how to handle 3D graphs ?
	if constexpr(dim<T> == 2)
		return {indexedEdges(), indexedPoints(), edgeValids(), vertexValids()};
	else {
		auto points2d = fwk::planarProjection<Point>(m_points, m_verts.valids(), m_projection);
		return {indexedEdges(), points2d, edgeValids(), vertexValids()};
	}
	return {};
}

// TODO: computation in 3D or pass Axes2D?
template <class T> vector<EdgeId> GeomGraph<T>::findIntersectors() const {
	vector<EdgeId> out;

	if constexpr(dim<T> == 3) {
		FATAL("write me");
	}

	auto grid = makeGrid();

	for(auto cell_id : grid) {
		if(!grid.empty(cell_id)) {
			auto edges = grid.cellEdges(cell_id);
			auto verts = grid.cellVerts(cell_id);

			for(int e1 : intRange(edges)) {
				EdgeRef edge1(ref(EdgeId(edges[e1])));
				Segment seg1 = (*this)(edge1);
				bool is_isecting = false;

				for(int e2 : intRange(e1 + 1, edges.size())) {
					EdgeId edge2(edges[e2]);
					if(!edge1.adjacent(edge2)) {
						Segment seg2 = (*this)(edge2);
						if(seg1.classifyIsect(seg2) != IsectClass::none) {
							is_isecting = true;
							out.push_back(edge2);
						}
					}
				}

				if(!is_isecting)
					for(VertexId node : verts) {
						if(!edge1.adjacent(node)) {
							if(seg1.classifyIsect((*this)(node)) != IsectClass::none) {
								is_isecting = true;
								break;
							}
						}
					}

				if(is_isecting)
					out.push_back(edge1);
			}
		}
	}

	return out;
}

template <class T> Ex<void> GeomGraph<T>::checkPlanar() const {
	auto isectors = findIntersectors();
	if(isectors) {
		auto error = ERROR("Graph not planar");
		auto &graph = *this;
		auto func = [&](EdgeId edge_id) {
			auto edge = graph.ref(edge_id);
			auto p1 = graph(edge.from()), p2 = graph(edge.to());
			return Segment{VecD(p1), VecD(p2)} * scale;
		};
		error.values.emplace_back(transform(isectors, func));
		return error;
	}
	return {};
}

template class GeomGraph<float2>;
template class GeomGraph<int2>;
template class GeomGraph<double2>;

template class GeomGraph<float3>;
template class GeomGraph<int3>;
template class GeomGraph<double3>;

template auto GeomGraph<float2>::toIntegral(double) const -> Ex<GeomGraph<int2>>;
template auto GeomGraph<float3>::toIntegral(double) const -> Ex<GeomGraph<int3>>;

template auto GeomGraph<double2>::toIntegral(double) const -> Ex<GeomGraph<int2>>;
template auto GeomGraph<double3>::toIntegral(double) const -> Ex<GeomGraph<int3>>;
}
