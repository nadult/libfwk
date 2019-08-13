// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/delaunay.h"

#include "fwk/geom/plane_graph.h"
#include "fwk/geom/plane_graph_builder.h"
#include "fwk/geom/segment_grid.h"
#include "fwk/geom/voronoi.h"
#include "fwk/math/direction.h"
#include "fwk/math/triangle.h"
#include "fwk/perf_base.h"
#include "fwk/small_vector.h"
#include "fwk/sys/assert.h"
#include "fwk/vector.h"
#include <numeric>

namespace fwk {

vector<VertexIdPair> delaunay(const VoronoiDiagram &voronoi) {
	auto &graph = voronoi.graph();
	auto cell_verts = transform<VertexId>(graph.verts(voronoi.site_layer));

	vector<Pair<VertexId>> out;
	vector<bool> visited(graph.numEdges(VoronoiDiagram::arc_layer));

	for(auto arc : graph.edges(VoronoiDiagram::arc_layer)) {
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

template <class T, EnableIfVec<T, 2>...> vector<VertexIdPair> delaunay(CSpan<T> points) {
	if constexpr(is_fpt<Base<T>>) {
		// Because of the conversion to/from int's, it's really approximate Delaunay
		// (although it's a very good approximation)
		auto scale = bestIntegralScale(points);
		auto ipoints = tryToIntegral<int2, T>(points, scale);
		DASSERT(ipoints);
		if(!ipoints)
			return {};

		return delaunay<int2>(*ipoints);
	} else {
		static_assert(is_integral<Base<T>>);
		return VoronoiDiagram::delaunay(points);
	}
}

// - find intersections
// - for each isect:
//   - swap each isected edge until there are none
//   - time bound ??

// Testy: -czy przecina (classifyIsect)
//        - czy czworokąt jest wypukły czy nie (cross-product)
//        - czy jest delaunay ?

// Positive convex means additionally that no two adjacent edges
// are parallel to each other
template <class T> bool isPositiveConvexQuad(CSpan<T, 4> points) {
	bool signs[4];
	for(auto [i, j, k] : wrappedTriplesRange(4)) {
		auto vec1 = points[j] - points[i];
		auto vec2 = points[k] - points[j];
		//print("v1:% v2:%\n", vec1, vec2);
		auto value = cross<llint2>(vec1, vec2);
		if(value == 0)
			return false;
		signs[i] = value < 0;
	}
	//print("signs: % % % %\n", signs[0], signs[1], signs[2], signs[3]);
	return signs[0] == signs[1] && signs[1] == signs[2] && signs[2] == signs[3];
}

// Algorithm source: Wykobi
static int insideCircumcircle(const int2 &p1, const int2 &p2, const int2 &p3, const int2 &p) {
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

template <class T, class Scalar = typename T::Scalar> auto polygonArea(CSpan<T> points) {
	Scalar area(0);

	for(auto [i, j, k] : wrappedTriplesRange(points)) {
		auto v0 = points[i], v1 = points[j], v2 = points[k];
		area += Scalar(v1[0]) * Scalar(v2[1] - v0[1]);
	}

	return std::abs(area / Scalar(2));
}
void testDelaunayFuncs() {
	int2 quad1[4] = {{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};
	ASSERT(isPositiveConvexQuad<int2>(quad1));

	int2 quad2[4] = {{0, 0}, {3, -4}, {6, 1}, {2, 6}};
	ASSERT(isPositiveConvexQuad<int2>(quad2));

	int2 quad3[4] = {{3, 0}, {0, 6}, {0, 0}, {-2, -5}};
	ASSERT(!isPositiveConvexQuad<int2>(quad3));

	int2 quad4[4] = {{0, 0}, {2, 1}, {0, 2}, {0, 1}};
	ASSERT(!isPositiveConvexQuad<int2>(quad4));

	ASSERT(!insideCircumcircle(quad1[0], quad1[1], quad1[2], quad1[3]));
	ASSERT(insideCircumcircle(quad1[0], quad1[1], quad1[2], int2(5000, 5000)));

	int2 vectors[6] = {{2, 3}, {-2, 3}, {-3, 0}, {-4, -2}, {0, -2}, {3, -2}};
	ASSERT_EQ(selectCCWMaxAngle(vectors[0], CSpan<int2>(vectors + 1, 5)), 2);
	ASSERT_EQ(selectCWMaxAngle(vectors[0], CSpan<int2>(vectors + 1, 5)), 3);

	int2 points[4] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
	ASSERT_EQ(polygonArea<int2>(points), 100);

	print("All OK!\n");
	exit(0);
}

template <class IT> struct CDT {
	using Seg = Segment<IT>;

	CDT(const PGraph<IT> &graph)
		: graph(graph), grid(graph.hasGrid() ? graph.grid() : graph.makeGrid()) {
		map.resize(graph.numVerts());
	}

	bool isIntersecting(const VertexIdPair &seg) const {
		Seg segment(graph[seg.first], graph[seg.second]);
		return graph.isectAnyEdge(grid, segment, IsectClass::point);
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

		int2 p1 = graph[from], p2 = graph[to];
		int2 vec = p2 - p1;

		SmallVector<int2, 16> vecs;
		SmallVector<VertexId, 16> ids;
		for(auto n : map[to])
			if(n != from) {
				vecs.emplace_back(graph[VertexId(n)] - p2);
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

	const PGraph<IT> &graph;
	SegmentGrid<IT> grid;
	vector<SmallVector<int>> map;
};

template <class IT, EnableIfIntegralVec<IT, 2>...>
vector<VertexIdPair> constrainedDelaunay(const PGraph<IT> &igraph, CSpan<VertexIdPair> dpairs) {
	PERF_SCOPE();

	CDT<IT> cdt(igraph);

	vector<pair<int, int>> invalid_pairs;
	vector<VertexIdPair> out;

	DASSERT(igraph.isPlanar(cdt.grid));
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

			IT qpoints[4] = {igraph[qverts[0]], igraph[qverts[1]], igraph[qverts[2]],
							 igraph[qverts[3]]};

			if(isPositiveConvexQuad<int2>(qpoints)) {
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

		auto remove_it = std::remove_if(begin(invalid_pairs), end(invalid_pairs),
										[](auto &pair) { return pair.first == sentinel; });
		invalid_pairs.resize(remove_it - invalid_pairs.begin());
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

			IT qpoints[4] = {igraph[qverts[0]], igraph[qverts[1]], igraph[qverts[2]],
							 igraph[qverts[3]]};

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

		auto end_it = std::remove_if(begin(new_pairs), end(new_pairs),
									 [](auto &pair) { return pair.first == sentinel; });
		new_pairs.resize(end_it - new_pairs.begin());
	}

#ifndef NDEBUG
	int num_errors = 0;
	for(auto pair : out)
		if(cdt.isIntersecting(pair))
			num_errors++;
	DASSERT_EQ(num_errors, 0);
#endif

	int num_free = 0;
	for(auto nref : igraph.vertexRefs())
		if(nref.numEdges() == 0)
			num_free++;
	//print("CDT: % edges + % points -> % edges\n", igraph.numEdges(), num_free, out.size());

	return out;
}

template <class T, EnableIfFptVec<T, 2>...>
vector<VertexIdPair> constrainedDelaunay(const PGraph<T> &graph, CSpan<VertexIdPair> delaunay) {
	// Because of the conversion to/from int's, it's really approximate Delaunay
	// (although it's a very good approximation)

	auto scale = bestIntegralScale(graph.points(), 512 * 1024 * 1024);
	auto igraph = toIntegral<int2, T>(graph, scale);
	DASSERT_EQ(igraph.numVerts(), graph.numVerts());
	return constrainedDelaunay<int2>(igraph, delaunay);
}

template <class T> bool isForestOfLoops(const PGraph<T> &pgraph) {
	for(auto nref : pgraph.vertexRefs())
		if(nref.numEdgesFrom() != 1 || nref.numEdgesTo() != 1)
			return false;
	vector<bool> visited(pgraph.numEdges(), false);

	for(auto estart : pgraph.edgeRefs()) {
		if(visited[estart])
			continue;
		visited[estart] = true;

		int count = 1;
		auto nref = estart.to();
		while(nref != estart.from()) {
			auto eref = nref.edgesFrom()[0];
			visited[eref] = true;
			nref = eref.to();
			count++;
		}

		if(count < 3)
			return false;
	}

	return true;
}

template <class T, EnableIfIntegralVec<T, 2>...>
vector<VertexIdPair> cdtFilterSide(const PGraph<T> &igraph, CSpan<VertexIdPair> cdt,
								   bool ccw_side) {
	DASSERT(igraph.isPlanar());
	PERF_SCOPE();

	PGraphBuilder<int2> builder(igraph.points());
	for(auto epair : igraph.edgePairs())
		builder(epair.first, epair.second);
	int cdt_offset = builder.numEdges();

	for(auto epair : cdt)
		if(!builder.find(epair.second, epair.first))
			builder(epair.first, epair.second);
	PGraph<int2> igraph2 = builder.build();
	vector<bool> enable(igraph2.numEdges(), false);

	vector<int> edge_ids;
	vector<int2> edge_dirs;

	for(auto start_edge : igraph2.edgeRefs()) {
		if(start_edge >= cdt_offset)
			continue;
		auto nref = start_edge.from();
		auto erefs = nref.edges();
		edge_ids.resize(erefs.size());
		std::iota(edge_ids.begin(), edge_ids.end(), 0);
		edge_dirs.clear();
		int2 zero_dir = igraph2[start_edge.to()] - igraph2[nref];
		for(auto eref : erefs)
			edge_dirs.push_back(igraph2[eref.other(nref)] - igraph2[nref]);
		orderByDirection<int2>(edge_ids, edge_dirs, zero_dir);
		if(!ccw_side)
			std::reverse(begin(edge_ids), end(edge_ids));

		enable[start_edge] = true;
		for(int idx : edge_ids) {
			if(erefs[idx] < cdt_offset)
				break;
			enable[erefs[idx]] = true;
		}
	}

	vector<bool> finished_verts(igraph.numVerts(), false);
	for(auto eref : igraph.edgeRefs())
		finished_verts[eref.from()] = finished_verts[eref.to()] = true;

	// Propagating to all the edges not directly reachable from constrained ones
	vector<VertexId> queue;
	for(auto eref : igraph2.edgeRefs()) {
		if(!enable[eref])
			continue;
		queue.emplace_back(eref.from());
		queue.emplace_back(eref.to());

		while(queue) {
			auto vert = igraph2.ref(queue.back());
			queue.pop_back();
			if(finished_verts[vert])
				continue;
			finished_verts[vert] = true;
			for(auto eref : vert.edges()) {
				enable[eref] = true;
				if(auto v2 = eref.other(vert); !finished_verts[v2])
					queue.emplace_back(v2);
			}
		}
	}

	vector<VertexIdPair> out;
	out.reserve(countIf(enable, [](bool v) { return v; }));
	for(auto eid : igraph2.edgeIds())
		if(enable[eid])
			out.emplace_back(igraph2.from(eid), igraph2.to(eid));

	/* TODO: reenable this
	auto func = [&](vis::Visualizer2 &vis, double2) {
		for(auto eref : igraph2.edgeRefs()) {
			if(enable[eref] || eref < cdt_offset)
				vis.drawLine(igraph2[eref.from()], igraph2[eref.to()],
							 eref < cdt_offset ? ColorId::white : ColorId::black);
			else
				vis.drawLine(igraph2[eref.from()], igraph2[eref.to()], ColorId::red);
		}
		return "Red triangles are removed";
	};
	//investigate(func, none, none);*/

	return out;
}

template <class T, EnableIfIntegralVec<T, 2>...>
vector<array<int, 3>> delaunaySideTriangles(const PGraph<T> &igraph, CSpan<VertexIdPair> cdt,
											CSpan<VertexIdPair> filter, bool ccw_side) {
	PERF_SCOPE();

	PGraphBuilder<T> builder(igraph.points());

	for(auto eref : igraph.edgeRefs()) {
		builder(igraph[eref].from, igraph[eref].to);
		builder(igraph[eref].to, igraph[eref].from);
	}
	for(auto [v1, v2] : cdt) {
		builder(igraph[v1], igraph[v2]);
		builder(igraph[v2], igraph[v1]);
	}
	auto graph = builder.build();
	CSpan<T> points = igraph.points();
	vector<bool> visited(graph.numEdges(), true);

	for(auto [v1, v2] : filter) {
		visited[*graph.findEdge(v1, v2)] = false;
		visited[*graph.findEdge(v2, v1)] = false;
	}

	vector<array<int, 3>> out;
	out.reserve(graph.numEdges() / 3);

	for(auto v1 : graph.vertexRefs()) {
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

template <class T, EnableIfFptVec<T, 2>...>
vector<Segment<T>> delaunaySegments(CSpan<VertexIdPair> pairs, const PGraph<T> &graph) {
	return transform(pairs, [&](const auto &pair) {
		return Segment2<typename T::Scalar>(graph[pair.first], graph[pair.second]);
	});
}

template <class Func>
static void delaunayTriangles(const ImmutableGraph &tgraph, const Func &feed_func) {
	vector<VertexId> buffer1, buffer2;
	buffer1.reserve(32);
	buffer2.reserve(32);
	vector<bool> visited(tgraph.numEdges(), false);

	for(auto eref : tgraph.edgeRefs()) {
		auto v1 = eref.from(), v2 = eref.to();
		buffer1.clear();
		buffer2.clear();

		for(auto eref1 : v1.edges())
			if(!visited[eref1])
				buffer1.emplace_back(eref1.other(v1).id());
		for(auto eref2 : v2.edges())
			if(!visited[eref2])
				buffer2.emplace_back(eref2.other(v2).id());
		for(auto i1 : buffer1)
			for(auto i2 : buffer2)
				if(i1 == i2) {
					// TODO: ordering?
					feed_func(v1, v2, i1);
				}
		visited[eref] = true;
	}
}

template <class T, class TReal, int N, EnableIfFptVec<T>...>
vector<Triangle<TReal, N>> delaunayTriangles(CSpan<VertexIdPair> pairs, CSpan<T> points) {
	PERF_SCOPE();
	vector<Triangle<TReal, N>> out;
	// TODO: reserve
	ImmutableGraph tgraph(pairs, points.size());
	delaunayTriangles(
		tgraph, [&](int a, int b, int c) { out.emplace_back(points[a], points[b], points[c]); });
	return out;
}

vector<array<int, 3>> delaunayTriangles(CSpan<VertexIdPair> pairs) {
	vector<array<int, 3>> out;
	// TODO: reserve
	ImmutableGraph tgraph(pairs);
	delaunayTriangles(tgraph, [&](int a, int b, int c) { out.emplace_back(a, b, c); });
	return out;
}

#define INSTANTIATE_INT(T)                                                                         \
	template vector<VertexIdPair> delaunay(CSpan<T>);                                              \
	template vector<array<int, 3>> delaunaySideTriangles(const PGraph<T> &, CSpan<VertexIdPair>,   \
														 CSpan<VertexIdPair>, bool);               \
	template vector<VertexIdPair> cdtFilterSide(const PGraph<T> &, CSpan<VertexIdPair>, bool);

#define INSTANTIATE(vec, seg, tri)                                                                 \
	template vector<seg> delaunaySegments(CSpan<VertexIdPair>, const PGraph<vec> &);               \
	template vector<tri> delaunayTriangles(CSpan<VertexIdPair>, CSpan<vec>);                       \
	template vector<VertexIdPair> delaunay(CSpan<vec>);                                            \
	template vector<VertexIdPair> constrainedDelaunay(const PGraph<vec> &, CSpan<VertexIdPair>);

INSTANTIATE(float2, Segment2F, Triangle2F)
INSTANTIATE(double2, Segment2D, Triangle2D)

INSTANTIATE_INT(int2)

template vector<Triangle3F> delaunayTriangles(CSpan<VertexIdPair>, CSpan<float3>);
template vector<Triangle3D> delaunayTriangles(CSpan<VertexIdPair>, CSpan<double3>);
}
