// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/delaunay.h"

#include "fwk/geom/segment_grid.h"
#include "fwk/geom/voronoi.h"
#include "fwk/hash_set.h"
#include "fwk/math/direction.h"
#include "fwk/math/qint.h"
#include "fwk/math/triangle.h"
#include "fwk/perf_base.h"
#include "fwk/small_vector.h"
#include "fwk/sys/assert.h"
#include "fwk/vector.h"
#include <numeric>

namespace fwk {

template <class T> double delaunayIntegralScale(CSpan<T> points) {
	return integralScale(enclose(points), delaunay_integral_resolution);
}

vector<VertexIdPair> delaunay(const Voronoi &voronoi) {
	auto &graph = voronoi.graph;
	auto cell_verts = graph.verts(voronoi.site_layer);

	vector<VertexIdPair> out;
	vector<bool> visited(graph.numEdges(Voronoi::arc_layer));

	for(auto arc : graph.edges(Voronoi::arc_layer)) {
		if(visited[arc])
			continue;
		DASSERT(arc.twin());

		auto cell1 = voronoi.cellId(arc);
		auto cell2 = voronoi.cellId(*arc.twin());

		auto vert1 = cell_verts[cell1];
		auto vert2 = cell_verts[cell2];
		out.emplace_back(vert1, vert2);
		visited[*arc.twin()] = true;
	}

	return out;
}

vector<VertexIdPair> delaunay(SparseSpan<int2> points) { return Voronoi::delaunay(points); }

template <c_vec<2> T> Ex<vector<VertexIdPair>> delaunay(CSpan<T> points) {
	auto scale = delaunayIntegralScale(points);
	auto ipoints = EX_PASS(toIntegral(points, scale));
	return Voronoi::delaunay(ipoints);
}

bool isPositiveConvexQuad(CSpan<int2, 4> corners) {
	bool signs[4];
	for(auto [i, j, k] : wrappedTriplesRange(4)) {
		auto vec1 = corners[j] - corners[i];
		auto vec2 = corners[k] - corners[j];
		auto value = cross<llint2>(vec1, vec2);
		if(value == 0)
			return false;
		signs[i] = value < 0;
	}
	return signs[0] == signs[1] && signs[1] == signs[2] && signs[2] == signs[3];
}

// Algorithm source: Wykobi
bool insideCircumcircle(const int2 &p1, const int2 &p2, const int2 &p3, const int2 &p) {
	llint dx1 = p1.x - p.x;
	llint dy1 = p1.y - p.y;
	llint dx2 = p2.x - p.x;
	llint dy2 = p2.y - p.y;
	llint dx3 = p3.x - p.x;
	llint dy3 = p3.y - p.y;

	qint det1 = dx1 * dy2 - dx2 * dy1;
	qint det2 = dx2 * dy3 - dx3 * dy2;
	qint det3 = dx3 * dy1 - dx1 * dy3;
	qint lift1 = dx1 * dx1 + dy1 * dy1;
	qint lift2 = dx2 * dx2 + dy2 * dy2;
	qint lift3 = dx3 * dx3 + dy3 * dy3;

	auto result = lift1 * det2 + lift2 * det3 + lift3 * det1;
	return result > qint(0);
}

int selectCCWMaxAngle(int2 vec1, CSpan<int2> vecs) {
	int best = -1;

	for(int i : intRange(vecs))
		if(cross<llint2>(vec1, vecs[i]) > 0) {
			best = i;
			break;
		}

	if(best == -1)
		return -1;

	for(int i : intRange(best + 1, vecs.size()))
		if(cross<llint2>(vec1, vecs[i]) > 0)
			if(cross<llint2>(vecs[best], vecs[i]) > 0)
				best = i;

	return best;
}

int selectCWMaxAngle(int2 vec1, CSpan<int2> vecs) {
	int best = -1;

	for(int i : intRange(vecs))
		if(cross<llint2>(vec1, vecs[i]) < 0) {
			best = i;
			break;
		}

	if(best == -1)
		return -1;

	for(int i : intRange(best + 1, vecs.size()))
		if(cross<llint2>(vec1, vecs[i]) < 0)
			if(cross<llint2>(vecs[best], vecs[i]) < 0)
				best = i;

	return best;
}

llint polygonArea(CSpan<int2> points) {
	llint area = 0;
	for(auto [i, j, k] : wrappedTriplesRange(points)) {
		auto v0 = points[i], v1 = points[j], v2 = points[k];
		area += v1[0] * llint(v2[1] - v0[1]);
	}

	return std::abs(area / (2));
}

struct CDT {
	using Seg = Segment2I;

	CDT(const GeomGraph<int2> &graph) : graph(graph), grid(graph.edgePairs(), graph.points()) {
		map.resize(graph.numVerts());
	}

	bool isIntersecting(const VertexIdPair &vertex_pair) const {
		Seg segment(graph(vertex_pair.first), graph(vertex_pair.second));
		for(auto cell : grid.trace(segment))
			for(auto eid : grid.cellEdges(cell)) {
				if(graph(eid).classifyIsect(segment) & IsectClass::point)
					return true;
			}
		return false;
	}

	void removeFrom(SmallVector<int> &vec, int idx) {
		for(int i : intRange(vec))
			if(vec[i] == idx) {
				vec[i] = vec.back();
				vec.pop_back();
				return;
			}
	}

	void removePair(pair<int, int> edge) {
		removeFrom(map[edge.first], edge.second);
		removeFrom(map[edge.second], edge.first);
	}

	void addPair(pair<int, int> edge) {
		map[edge.first].emplace_back(edge.second);
		map[edge.second].emplace_back(edge.first);
	}

	// Quad: seg[0], cw, seg[1], ccw
	array<VertexId, 4> getQuad(VertexId from, VertexId to) const {
		array<VertexId, 4> out = {from, to, no_init, no_init};
		int found = 0;

		int2 p1 = graph(from), p2 = graph(to);
		int2 vec = p2 - p1;

		SmallVector<int2, 16> vecs;
		SmallVector<VertexId, 16> ids;
		for(auto n : map[to])
			if(n != from) {
				vecs.emplace_back(graph(VertexId(n)) - p2);
				ids.emplace_back(n);
			}

		int cw_id = selectCWMaxAngle(vec, vecs);
		int ccw_id = selectCCWMaxAngle(vec, vecs);
		DASSERT(cw_id != -1 && ccw_id != -1);

		VertexId sel_cw = ids[cw_id];
		VertexId sel_ccw = ids[ccw_id];
		DASSERT_NE(sel_cw, sel_ccw);

		out[0] = from;
		out[1] = sel_cw;
		out[2] = to;
		out[3] = sel_ccw;
		return out;
	}

	const GeomGraph<int2> &graph;
	SegmentGrid<int2> grid;
	vector<SmallVector<int>> map;
};

vector<VertexIdPair> constrainedDelaunay(const GeomGraph<int2> &igraph,
										 CSpan<VertexIdPair> dpairs) {
	PERF_SCOPE();

	CDT cdt(igraph);

	vector<pair<int, int>> invalid_pairs;
	vector<VertexIdPair> out;

	//DASSERT(igraph.isPlanar(cdt.grid)); // TODO: fix it
	vector<VertexIdPair> temp;
	if(dpairs.empty()) {
		temp = delaunay(igraph.points());
		dpairs = temp;
	}

	for(auto pair : dpairs) {
		if(cdt.isIntersecting(pair))
			invalid_pairs.emplace_back(pair);
		else
			out.emplace_back(pair);
		cdt.addPair(pair);
	}

	vector<VertexId> buffer1, buffer2;
	buffer1.reserve(32);
	buffer2.reserve(32);

	// Algortihm: Sloan93
	vector<pair<int, int>> new_pairs;

	static constexpr int sentinel = -1;
	while(!invalid_pairs.empty()) {
		for(int n : intRange(invalid_pairs)) {
			auto cur_pair = invalid_pairs[n];
			auto qverts = cdt.getQuad(VertexId(cur_pair.first), VertexId(cur_pair.second));

			int2 qpoints[4] = {igraph(qverts[0]), igraph(qverts[1]), igraph(qverts[2]),
							   igraph(qverts[3])};

			if(isPositiveConvexQuad(qpoints)) {
				// print("convex!\n");
				VertexIdPair flipped_pair(qverts[1], qverts[3]);
				cdt.removePair(cur_pair);
				cdt.addPair(flipped_pair);

				if(cdt.isIntersecting(flipped_pair)) {
					invalid_pairs[n] = flipped_pair;
				} else {
					new_pairs.emplace_back(flipped_pair);
					invalid_pairs[n].first = sentinel;
				}
			} else {
				// print("concave :(\n");
				DASSERT_GT(invalid_pairs.size(), 1);
			}
		}

		removeIf(invalid_pairs, [](auto &pair) { return pair.first == sentinel; });
	}

	while(!new_pairs.empty()) {
		for(int i : intRange(new_pairs)) {
			VertexIdPair cur_pair(new_pairs[i].first, new_pairs[i].second);
			if(igraph.findEdge(cur_pair.first, cur_pair.second)) {
				out.emplace_back(cur_pair);
				new_pairs[i].first = sentinel;
				continue;
			}
			if(igraph.findEdge(cur_pair.second, cur_pair.first)) {
				out.emplace_back(cur_pair.second, cur_pair.first);
				new_pairs[i].first = sentinel;
				continue;
			}

			auto qverts = cdt.getQuad(cur_pair.first, cur_pair.second);

			int2 qpoints[4] = {igraph(qverts[0]), igraph(qverts[1]), igraph(qverts[2]),
							   igraph(qverts[3])};

			// Are both tests required ?
			bool test1 = insideCircumcircle(qpoints[0], qpoints[1], qpoints[2], qpoints[3]);
			bool test2 = insideCircumcircle(qpoints[0], qpoints[2], qpoints[3], qpoints[1]);

			if(test1 || test2) {
				VertexIdPair new_pair(qverts[1], qverts[3]);
				cdt.removePair(cur_pair);
				cdt.addPair(new_pair);
				out.emplace_back(new_pair);
				new_pairs[i].first = sentinel;
			} else {
				out.emplace_back(cur_pair);
				new_pairs[i].first = sentinel;
			}
		}

		removeIf(new_pairs, [](auto &pair) { return pair.first == sentinel; });
	}

#ifndef NDEBUG
	int num_errors = 0;
	for(auto pair : out)
		if(cdt.isIntersecting(pair))
			num_errors++;
	DASSERT_EQ(num_errors, 0);
#endif

	int num_free = 0;
	for(auto vert : igraph.verts())
		if(vert.numEdges() == 0)
			num_free++;
	//print("CDT: % edges + % points -> % edges\n", igraph.numEdges(), num_free, out.size());

	return out;
}

template <c_vec<2> T>
Ex<vector<VertexIdPair>> constrainedDelaunay(const GeomGraph<T> &graph,
											 CSpan<VertexIdPair> delaunay) {
	// Because of the conversion to/from int's, it's really approximate Delaunay
	// (although it's a very good approximation)

	auto scale = integralScale(graph.boundingBox(), delaunay_integral_resolution);
	auto igraph = EX_PASS(graph.toIntegral(scale));
	return constrainedDelaunay(igraph, delaunay);
}

template <class T> bool isForestOfLoops(const GeomGraph<T> &pgraph) {
	for(auto vert : pgraph.vertexRefs())
		if(vert.numEdgesFrom() != 1 || vert.numEdgesTo() != 1)
			return false;
	vector<bool> visited(pgraph.numEdges(), false);

	for(auto estart : pgraph.edges()) {
		if(visited[estart])
			continue;
		visited[estart] = true;

		int count = 1;
		auto vert = estart.to();
		while(vert != estart.from()) {
			auto edge = vert.edgesFrom()[0];
			visited[edge] = true;
			vert = edge.to();
			count++;
		}

		if(count < 3)
			return false;
	}

	return true;
}

template <c_vec<2> T>
void orderByDirection(Span<int> indices, CSpan<T> vectors, const T &zero_vector) {
	using PT = PromoteIntegral<T>;
	auto it = std::partition(begin(indices), end(indices), [=](int id) {
		auto tcross = cross<PT>(vectors[id], zero_vector);
		if(tcross == 0) {
			return dot<PT>(vectors[id], zero_vector) > 0;
		}
		return tcross < 0;
	});

	auto func1 = [=](int a, int b) { return cross<PT>(vectors[a], vectors[b]) > 0; };
	auto func2 = [=](int a, int b) { return cross<PT>(vectors[a], vectors[b]) > 0; };

	std::sort(begin(indices), it, func1);
	std::sort(it, end(indices), func2);
}

vector<VertexIdPair> cdtFilterSide(CSpan<int2> points, CSpan<VertexIdPair> cdt, bool ccw_side) {
	// TODO: make it robust
	GeomGraph<int2> temp;
	for(auto pt : points)
		temp.fixVertex(pt);
	return cdtFilterSide(temp, cdt, ccw_side);
}

vector<VertexIdPair> cdtFilterSide(const GeomGraph<int2> &igraph, CSpan<VertexIdPair> cdt,
								   bool ccw_side) {
	//DASSERT(igraph.isPlanar()); // TODO: make it work
	PERF_SCOPE();

	FATAL("test me");
	GeomGraph<int2> temp;
	for(auto vert : igraph.verts())
		temp.addVertexAt(vert, igraph(vert));
	for(auto edge : igraph.edges())
		temp.addEdgeAt(edge, edge.from(), edge.to());
	for(auto epair : cdt)
		temp.fixEdge(epair.first, epair.second);

	vector<bool> enable(temp.edgesSpread(), false);
	vector<int> edge_ids;
	vector<int2> edge_dirs;

	for(auto start_edge : temp.edges()) {
		if(!igraph.valid(start_edge.id()))
			continue;

		auto vert = start_edge.from();
		auto edges = vert.edges();

		edge_ids.resize(edges.size());
		std::iota(edge_ids.begin(), edge_ids.end(), 0);
		edge_dirs.clear();
		int2 zero_dir = temp(start_edge.to()) - temp(vert);
		for(auto eref : edges)
			edge_dirs.push_back(temp(eref.other(vert)) - temp(vert));
		orderByDirection<int2>(edge_ids, edge_dirs, zero_dir);
		if(!ccw_side)
			reverse(edge_ids);

		enable[start_edge] = true;
		for(int idx : edge_ids) {
			if(igraph.valid(edges[idx]))
				break;
			enable[edges[idx]] = true;
		}
	}

	vector<bool> finished_verts(igraph.vertsSpread(), false);
	for(auto edge : igraph.edges())
		finished_verts[edge.from()] = finished_verts[edge.to()] = true;

	// Propagating to all the edges not directly reachable from constrained ones
	vector<VertexId> queue;
	for(auto edge : temp.edges()) {
		if(!enable[edge])
			continue;
		queue.emplace_back(edge.from());
		queue.emplace_back(edge.to());

		while(queue) {
			auto vert = temp.ref(queue.back());
			queue.pop_back();
			if(finished_verts[vert])
				continue;
			finished_verts[vert] = true;
			for(auto edge : vert.edges()) {
				enable[edge] = true;
				if(auto v2 = edge.other(vert); !finished_verts[v2])
					queue.emplace_back(v2);
			}
		}
	}

	vector<VertexIdPair> out;
	out.reserve(countIf(enable));
	for(auto eid : temp.edgeIds())
		if(enable[eid])
			out.emplace_back(temp.from(eid), temp.to(eid));

	/* TODO: reenable this
	auto func = [&](vis::Visualizer2 &vis, double2) {
		for(auto eref : temp.edges()) {
			if(enable[eref] || eref < cdt_offset)
				vis.drawLine(temp[eref.from()], temp[eref.to()],
							 eref < cdt_offset ? ColorId::white : ColorId::black);
			else
				vis.drawLine(temp[eref.from()], temp[eref.to()], ColorId::red);
		}
		return "Red triangles are removed";
	};
	//investigate(func, none, none);*/

	return out;
}

vector<array<int, 3>> delaunayTriangles(const GeomGraph<int2> &igraph, CSpan<VertexIdPair> cdt,
										CSpan<VertexIdPair> filter, bool ccw_side,
										bool invert_filter) {
	PERF_SCOPE();

	GeomGraph<int2> temp;
	for(auto vert : igraph.verts())
		temp.addVertexAt(vert, igraph(vert));

	for(auto edge : igraph.edges()) {
		temp.fixEdge(edge.from(), edge.to());
		temp.fixEdge(edge.to(), edge.from());
	}
	for(auto [v1, v2] : cdt) {
		temp.fixEdge(v1, v2);
		temp.fixEdge(v2, v1);
	}

	for(auto vert : temp.verts())
		temp.orderEdges(vert);

	vector<bool> visited(temp.edgesSpread(), !invert_filter);

	for(auto [v1, v2] : filter) {
		visited[*temp.findEdge(v1, v2)] = invert_filter;
		visited[*temp.findEdge(v2, v1)] = invert_filter;
	}

	vector<array<int, 3>> out;
	out.reserve(temp.numEdges() / 3);
	auto points = temp.indexedPoints();

	for(auto v1 : temp.verts()) {
		for(auto e1 : v1.edgesFrom()) {
			if(visited[e1])
				continue;

			auto e2 = e1.twin()->prevFrom();
			auto e3 = e2.twin()->prevFrom();

			if(!visited[e2] && !visited[e3] && e3.to() == v1) {
				visited[e2] = visited[e3] = true;
				out.emplace_back(v1, e2.from(), e3.from());

				auto &back = out.back();
				if(ccwSide(points[back[0]], points[back[1]], points[back[2]]) != ccw_side)
					back = {back[0], back[2], back[1]};
			}
		}
	}

	/* TODO: reenable this
	auto vfunc = [&](vis::Visualizer2 &vis, double2 mouse_pos) {
		vector<double2> points = transform<double2>(graph.points());
		pair<double, Triangle2D> closest = {inf, {}};
		for(int i : intRange(out)) {
			auto &ids = out[i];
			Triangle2D tri(points[ids[0]], points[ids[1]], points[ids[2]]);
			closest = min(closest, {distanceSq(tri.center(), mouse_pos), tri});
		}

		vis.drawPlaneGraph(graph, ColorId::red, ColorId::red);
		for(auto &tri : out) {
			for(auto [i, j] : wrappedPairsRange(3))
				vis.drawLine(points[tri[i]], points[tri[j]], ColorId::black);
		}

		vis.drawTriangle(closest.second, ColorId::white);
		return "";
	};
	//if(out)
	//	investigate(vfunc, none, none);*/

	return out;
}

template <c_float_vec<2> T>
vector<Segment<T>> delaunaySegments(CSpan<VertexIdPair> pairs, const GeomGraph<T> &graph) {
	return transform(pairs, [&](const auto &pair) {
		return Segment2<typename T::Scalar>(graph(pair.first), graph(pair.second));
	});
}

#define INSTANTIATE(vec, seg, tri)                                                                 \
	template vector<seg> delaunaySegments(CSpan<VertexIdPair>, const GeomGraph<vec> &);            \
	template Ex<vector<VertexIdPair>> delaunay(CSpan<vec>);                                        \
	template Ex<vector<VertexIdPair>> constrainedDelaunay(const GeomGraph<vec> &,                  \
														  CSpan<VertexIdPair>);                    \
	template double delaunayIntegralScale(CSpan<vec>);

INSTANTIATE(float2, Segment2F, Triangle2F)
INSTANTIATE(double2, Segment2D, Triangle2D)
}
