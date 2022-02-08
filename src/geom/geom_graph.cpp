// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/geom_graph.h"

#include "fwk/geom/segment_grid.h"
#include "fwk/math/direction.h"
#include "fwk/math/random.h"
#include "fwk/math/segment.h"
#include "fwk/math/triangle.h"
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
	vector<Maybe<VertexId>> collapses(graph.vertsSpread());
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

template <class T> GeomGraph<T>::GeomGraph(CSpan<Triangle> tris) {
	reserveVerts(tris.size() * 3);
	reserveEdges(tris.size() * 3);
	reserveTris(tris.size());

	for(auto tri : tris) {
		array<VertexId, 3> nodes = {
			{fixVertex(tri[0]).id, fixVertex(tri[1]).id, fixVertex(tri[2]).id}};

		// TODO: why are flips needed?
		array<bool, 3> flip = {{nodes[0] > nodes[1], nodes[1] > nodes[2], nodes[2] > nodes[0]}};

		fixEdge(nodes[flip[0] ? 1 : 0], nodes[flip[0] ? 0 : 1]);
		fixEdge(nodes[flip[0] ? 2 : 1], nodes[flip[0] ? 1 : 2]);
		fixEdge(nodes[flip[0] ? 0 : 2], nodes[flip[0] ? 2 : 0]);
		addTri(nodes[0], nodes[1], nodes[2]);
	}
}

template <class T> GeomGraph<T>::GeomGraph() = default;
template <class T> GeomGraph<T>::GeomGraph(const GeomGraph &) = default;
template <class T> GeomGraph<T>::GeomGraph(GeomGraph &&) = default;
template <class T> GeomGraph<T> &GeomGraph<T>::operator=(const GeomGraph &) = default;
template <class T> GeomGraph<T> &GeomGraph<T>::operator=(GeomGraph &&) = default;
template <class T> GeomGraph<T>::~GeomGraph() = default;

// -------------------------------------------------------------------------------------------
// ---  Access to graph elements -------------------------------------------------------------

template <class T> SparseSpan<T> GeomGraph<T>::points() const {
	return {m_points.data(), m_verts.valids(), m_verts.size()};
}

template <class T> Box<T> GeomGraph<T>::boundingBox() const { return enclose(points()); }

template <class T> vector<Segment<T>> GeomGraph<T>::segments() const {
	auto &graph = *this;
	return fwk::transform(edgeIds(), [&](EdgeId id) { return graph(id); });
}

template <class T> Segment<T> GeomGraph<T>::operator()(EdgeId id) const {
	auto [n1, n2] = m_edges[id];
	return {m_points[n1], m_points[n2]};
}

template <class T> auto GeomGraph<T>::operator()(TriangleId id) const -> Triangle {
	auto [n1, n2, n3] = m_tris[id].verts;
	return {m_points[n1], m_points[n2], m_points[n3]};
}

template <class T> T GeomGraph<T>::vec(EdgeId id) const {
	auto [n1, n2] = m_edges[id];
	return m_points[n2] - m_points[n1];
}

template <class T> Maybe<VertexRef> GeomGraph<T>::findVertex(Point pt) const {
	if(auto vid = findPoint(pt))
		return ref(*vid);
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
	if(auto vid = findPoint(point)) {
		m_vert_layers[*vid] |= layers;
		return {*vid, false};
	}
	auto id = Graph::addVertex(layers);
	m_points.resize(Graph::m_verts.capacity());
	m_points[id] = point;
	m_point_map[point] = id;
	return {id, true};
}

template <class T>
void GeomGraph<T>::mergeVerts(CSpan<VertexId> verts, const Point &point, Layers layers) {
	DASSERT(verts);

	if(auto vid = findPoint(point))
		DASSERT(isOneOf(*vid, verts));
	auto target = verts.front();

	// TODO: what to do with duplicated egdes & tris ?
	for(int i : intRange(1, verts.size())) {
		auto vert = ref(verts[i]);
		for(auto edge : vert.edges()) {
			bool is_source = edge.from() == vert;
			auto other = edge.other(vert);
			// TODO: labels
			remove(edge);
			if(!isOneOf(other.id(), verts)) {
				if(is_source)
					fixEdge(target, other);
				else
					fixEdge(other, target);
			}
		}
	}

	for(int i : intRange(1, verts.size()))
		remove(verts[i]);
	m_point_map[point] = target;
	m_points[target] = point;
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
		auto vec = flatten(m_points[edge.other(vid)] - current_pos, axes);
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
	DASSERT_GE(valid_indices.size(), points.size());

	PointMap point_map;
	point_map.reserve(points.size());
	for(auto vid : indexRange<VertexId>(valid_indices))
		if(valid_indices[vid]) {
			if(auto it = point_map.find(points[vid]))
				identical_points.emplace_back(vid, it->value);
			else
				point_map[points[vid]] = vid;
		}

	return point_map;
}

template <class T>
template <c_float_vec U>
auto GeomGraph<T>::toIntegral(double scale) const -> Ex<GeomGraph<IPoint>> {
	PodVector<IPoint> new_points(vertsSpread());
	for(auto vert : verts())
		new_points[vert] = IPoint((*this)(vert)*scale);
	auto out = replacePoints(new_points);
	if(out)
		out->scale = 1.0 / scale;
	return out;
}

template <class T>
template <c_float_vec U>
auto GeomGraph<T>::toIntegralWithCollapse(double scale) const -> GeomGraph<IPoint> {
	PodVector<IPoint> new_points(vertsSpread());
	for(auto vert : verts())
		new_points[vert] = IPoint((*this)(vert)*scale);
	auto out = replacePointsWithCollapse(new_points);
	out.scale = 1.0 / scale;
	return out;
}

// -------------------------------------------------------------------------------------------
// ---  Grid-based Algorithms ----------------------------------------------------------------

template <class T> auto GeomGraph<T>::makeGrid() const -> Grid {
	// TODO: how to handle 3D graphs ?
	if constexpr(dim<T> == 2)
		return {edgePairs(), points()};
	else {
		auto flat_points = flatten<Point>(points(), m_flat_axes);
		return {edgePairs(), move(flat_points), m_verts.valids(), m_verts.size()};
	}
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

template <class T> auto GeomGraph<T>::flatPoint(VertexId vid) const -> Vec2 {
	return flatten(m_points[vid], m_flat_axes); // TODO: naming sux
}

template <class T> auto GeomGraph<T>::flatSegment(EdgeId eid) const -> Segment2 {
	return flatten((*this)(eid), m_flat_axes);
}

// Czy to ma zwracać punkty double2 czy T ? doble2 można sobie skonwertować do intów (najwyżej będzie
// to konwersja w której trzeba będzie mergować ?
// Jak robimy grid, to przydałyby się punkty zrzutowane na płaszczyznę; Czy one mają być w gridzie,
// czy ... ?
//
// Co z grafem 3D? zwracamy punkty 2D? Czy grid nie powinien działać na doublach ?
template <class T>
vector<double2> GeomGraph<T>::randomPoints(Random &random, double min_dist,
										   Maybe<DRect> trect) const {
	DASSERT_GT(min_dist, 0.0);
	DRect rect = trect ? *trect : DRect(flatten(boundingBox(), m_flat_axes));

	RegularGrid<double2> ugrid(rect, min_dist / sqrt2, 1);
	auto min_dist_sq = min_dist * min_dist;

	double2 invalid_pt(inf);
	vector<double2> points(ugrid.width() * ugrid.height(), invalid_pt);

	vector<double2> out;
	out.reserve(ugrid.width() * ugrid.height() / 4);

	auto grid = makeGrid();

	for(auto pos : cells(ugrid.cellRect().inset(1))) {
		auto pt = ugrid.toWorld(pos) + random.sampleBox(double2(0), double2(1)) * ugrid.cellSize();

		int idx = pos.x + pos.y * ugrid.width();
		int indices[4] = {idx - 1, idx - ugrid.width(), idx - ugrid.width() - 1,
						  idx - ugrid.width() + 1};

		if(allOf(indices, [&](int idx) { return distanceSq(points[idx], pt) >= min_dist_sq; }))
			if(!grid.closestEdge(Vec2(pt), min_dist)) {
				points[idx] = pt;
				out.emplace_back(pt);
			}
	}

	return out;
}

template <class T> auto GeomGraph<T>::mergeNearby(double join_dist) const -> MergedVerts {
	// When computing on integers, you have to make sure not to overflow
	// when computing distance
	struct MergedPoint {
		VecD point;
		List verts;
		int num_verts;
	};
	vector<MergedPoint> merged;
	PodVector<ListNode> nodes(vertsSpread() * 2);
	auto acc_node = [&](int idx) -> ListNode & { return nodes[idx]; };
	vector<int> removed_merged;

	vector<bool> is_merged(vertsSpread(), false);
	// Differentiates old verts (< break_idx) from new merged verts
	int break_idx = vertsSpread();
	using Vec2I = MakeVec<int, 2>;

	auto inv_cell_size = 1.0 / join_dist;

	auto to_cell = [=](VecD pos) {
		auto fpos = flatten(pos, m_flat_axes);
		return Vec2I(vfloor(fpos * inv_cell_size));
	};

	auto join_dist_sq = join_dist * join_dist;
	auto cell_size = join_dist;

	// Mapping from cell to vertex list
	HashMap<int2, List> cell_verts;
	for(int id : vertexIds())
		listInsert(acc_node, cell_verts[to_cell(VecD(m_points[id]))], id);

	// indices >= break_idx point to merged points
	auto get_pos = [&](int id) {
		return id >= break_idx ? merged[id - break_idx].point : m_points[id];
	};

	// Jak dodać zmergowane punkty do list?
	// Jak przelecieć na końcu po zmergowanych punktach?

	auto try_merge = [&](int cur_id) -> bool {
		// TODO: compute on doubles ?
		auto cur_pos = VecD(get_pos(cur_id));
		double cur_weight = 1.0;
		auto cur_cell = to_cell(cur_pos);

		List new_list;
		int num_verts = 0;

		auto do_merge = [&](int vert, List &cell_list) {
			listRemove(acc_node, cell_list, vert);
			//print("Merge: %\n", vert);
			if(vert < break_idx) {
				DASSERT_EX(!is_merged[vert], vert);
				listInsert(acc_node, new_list, vert);
				is_merged[vert] = true;
				num_verts++;
			} else {
				int mid = vert - break_idx;
				listMerge(acc_node, new_list, merged[mid].verts);
				num_verts += merged[mid].num_verts;
				removed_merged.emplace_back(mid);
				merged[mid].num_verts = 0;
			}
		};

		for(auto npos : nearby9Cells(cur_cell)) {
			auto it = cell_verts.find(npos);
			if(!it)
				continue;

			for(int vert = it->value.head, next; vert != -1; vert = next) {
				next = nodes[vert].next;
				if(vert == cur_id)
					continue;

				auto vert_pos = VecD(get_pos(vert));
				auto dist_sq = distanceSq(cur_pos, (VecD)get_pos(vert));
				if(dist_sq < join_dist_sq) {
					int mid = vert - break_idx;
					double vert_weight = vert >= break_idx ? merged[mid].num_verts : 1;
					auto new_weight = vert_weight + cur_weight;
					cur_pos = (cur_pos * cur_weight + vert_pos * vert_weight) / new_weight;
					cur_weight = new_weight;

					do_merge(vert, it->value);
				}
			}
		}

		if(num_verts == 0)
			return false;

		do_merge(cur_id, cell_verts[cur_cell]);

		int mid = merged.size();
		if(removed_merged) {
			mid = removed_merged.back();
			removed_merged.pop_back();
			merged[mid] = {cur_pos, new_list, num_verts};
		} else
			merged.emplace_back(cur_pos, new_list, num_verts);
		//print("Merging into %: ", mid + break_idx);
		//for(int n = merged[mid].verts.head; n != -1; n = nodes[n].next)
		//	print("% ", n);
		//print("\n");
		listInsert(acc_node, cell_verts[to_cell(cur_pos)], mid + break_idx);

		return true;
	};

	bool needs_merge = false;
	for(int vid : vertexIds())
		if(!is_merged[vid])
			needs_merge |= try_merge(vid);
	while(needs_merge) {
		needs_merge = false;
		for(int mid : intRange(merged))
			if(merged[mid].num_verts)
				needs_merge |= try_merge(mid + break_idx);
	}

	MergedVerts out;
	int num_verts_merged = 0;
	for(int n : intRange(merged)) {
		if(!merged[n].num_verts)
			continue;

		int vid = merged[n].verts.head;
		num_verts_merged |= merged[n].num_verts;
		out.new_points.emplace_back(T(merged[n].point));
		out.num_verts.emplace_back(merged[n].num_verts);
		while(vid != -1) {
			out.indices.emplace_back(VertexId(vid));
			vid = nodes[vid].next;
		}
	}

	DASSERT(distinct(out.indices));
	return out;
}

template <class T> Maybe<VertexId> GeomGraph<T>::findPoint(const Point &point) const {
	if(auto it = m_point_map.find(point); it != m_point_map.end())
		return VertexId(it->value);
	return none;
}

#ifndef FWK_SKIP_INSTANTIATIONS
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
#endif
}
