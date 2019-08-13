// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/plane_graph.h"

#include "fwk/any.h"
#include "fwk/geom/contour.h"
#include "fwk/geom/order_edges.h"
#include "fwk/geom/planar_loops.h"
#include "fwk/geom/plane_graph_builder.h"
#include "fwk/geom/regular_grid.h"
#include "fwk/geom/segment_grid.h"
#include "fwk/hash_map.h"
#include "fwk/math/random.h"
#include "fwk/math/rational.h"
#include "fwk/pod_vector.h"
#include "fwk/small_vector.h"
#include "fwk/sys/error.h"
#include "fwk/variant.h"

namespace fwk {

template <class T> void PlaneGraph<T>::verify(const ImmutableGraph &igraph, CSpan<T> points) {
	DASSERT_EQ(points.size(), igraph.numVerts());
	DASSERT(distinct(points));
	// TODO: more checks
}

template <class T>
PlaneGraph<T>::PlaneGraph(CSpan<Pair<VertexId>> edges, vector<T> points)
	: ImmutableGraph(edges, points.size()), m_points(move(points)) {
	verify(*this, m_points);
	orderEdges();
	computeExtendedInfo();
}

template <class T>
PlaneGraph<T>::PlaneGraph(ImmutableGraph igraph, vector<T> points)
	: ImmutableGraph(move(igraph)), m_points(move(points)) {
	verify(*this, m_points);
	orderEdges();
	computeExtendedInfo();
}

template <class T> void PlaneGraph<T>::orderEdges() { fwk::orderEdges<T>(*this, m_points); }

template <class T>
template <class U, EnableIfFptVec<U>...>
PlaneGraph<T> PlaneGraph<T>::fromContours(CSpan<Contour<T>> contours) {
	Builder builder;
	for(auto &c : contours)
		for(auto seg : c.segments())
			builder(T(seg.from), T(seg.to));
	return builder.build();
}

template <class T>
template <class U, EnableIfFptVec<U>...>
PlaneGraph<T> PlaneGraph<T>::fromSegments(CSpan<Segment> segments) {
	Builder builder;
	for(auto segment : segments)
		builder(T(segment.from), T(segment.to));
	return builder.build();
}

template <class T> PlaneGraph<T>::PlaneGraph() = default;
template <class T> PlaneGraph<T>::~PlaneGraph() = default;
template <class T> PlaneGraph<T>::PlaneGraph(const PlaneGraph &) = default;
template <class T> PlaneGraph<T>::PlaneGraph(PlaneGraph &&) = default;
template <class T> PlaneGraph<T> &PlaneGraph<T>::operator=(const PlaneGraph &) = default;
template <class T> PlaneGraph<T> &PlaneGraph<T>::operator=(PlaneGraph &&) = default;

template <class T> auto PlaneGraph<T>::makeGrid() const -> Grid { return {edgePairs(), m_points}; }
template <class T> void PlaneGraph<T>::addGrid() { m_grid = {edgePairs(), m_points}; }
template <class T> void PlaneGraph<T>::setGrid(Grid grid) { m_grid = {move(grid)}; }

template <class T> void PlaneGraph<T>::apply(PointTransform trans, bool remove_collapsed_edges) {
	Builder builder((trans.new_points));
	DASSERT(builder.numVerts() == trans.new_points.size());
	builder.reserveEdges(numEdges());

	DASSERT_GE(trans.mapping.size(), numVerts());

	for(auto eref : edgeRefs()) {
		VertexId from(trans.mapping[eref.from()]), to(trans.mapping[eref.to()]);
		if(!remove_collapsed_edges || from != to)
			builder(from, to);
	}

	auto new_graph = builder.build();
	*((ImmutableGraph *)this) = new_graph;
	m_points = new_graph.m_points;
}

template <class T> pair<ImmutableGraph, vector<T>> PlaneGraph<T>::decompose(PlaneGraph<T> graph) {
	return {move(*(ImmutableGraph *)&graph), move(graph.m_points)};
}

// Does it have to work well for not-simple graphs ? currently no, but it should work 100% correctly
template <class T>
template <class U, EnableIfFptVec<U>...>
vector<Contour<T>> PlaneGraph<T>::contours() const {
	if(!isUndirected())
		return PlaneGraph<T>(asUndirected(), m_points).contours();

	// TODO: requirements for input ?
	vector<bool> visited(numEdges(), false);
	auto grades = fwk::transform(vertexRefs(), [](auto vref) { return vref.vertsAdj().size(); });

	vector<Contour<T>> out;
	vector<VertexId> line;

	for(auto eref : edgeRefs())
		if(eref.from() == eref.to())
			visited[eref] = true;

	for(int cycles_mode = 0; cycles_mode <= 1; cycles_mode++)
		for(auto eref : edgeRefs()) {
			if(visited[eref] || (grades[eref.from()] == 2 && !cycles_mode))
				continue;

			auto cur_edge = eref;
			auto start_node = eref.from();
			auto cur_node = eref.to();

			line.clear();
			line.emplace_back(start_node.id());

			while(true) {
				visited[cur_edge] = true;
				if(auto twin = cur_edge.twin())
					visited[*twin] = true;
				line.emplace_back(cur_node.id());

				if(cur_node == start_node || grades[cur_node] != 2)
					break;

				bool found = false;
				for(auto next : cur_node.edges())
					if(!visited[next]) {
						cur_edge = next;
						cur_node = next.other(cur_node);
						found = true;
						break;
					}

				if(!found)
					break;
			}

			bool is_looped = line.back() == line.front();
			if(is_looped)
				line.pop_back();

			out.emplace_back(fwk::transform(line, [&](auto nid) { return m_points[nid]; }),
							 is_looped);
		}

	return out;
}

template <class T> vector<vector<EdgeId>> PlaneGraph<T>::edgeLoops() const {
	return planarLoops<T>(*this, m_points);
}

// TODO: allow int-based contours
template <class T>
template <class U, EnableIfFptVec<U>...>
vector<Contour<T>> PlaneGraph<T>::contourLoops() const {
	if(!isUndirected())
		return PlaneGraph<T>(asUndirected(), m_points).contourLoops();

	auto edge_loops = edgeLoops();
	vector<Contour<T>> out;
	out.reserve(edge_loops.size());
	auto &graph = *this;
	for(auto &loop : edge_loops)
		out.emplace_back(fwk::transform(loop, [&](EdgeId id) { return graph[id]; }));

	return out;
}

template <class T>
SmallVector<EdgeId> PlaneGraph<T>::isectEdges(const Segment &seg, IsectFlags flags) const {
	DASSERT(m_grid);
	SmallVector<EdgeId> out;
	for(auto cell : m_grid->trace(seg)) {
		for(auto eid : m_grid->cellEdges(cell)) {
			if(flags & (*this)[eid].classifyIsect(seg))
				out.emplace_back(eid);
		}
	}

	return out;
}

template <class T>
bool PlaneGraph<T>::isectAnyEdge(const Grid &grid, const Segment &seg, IsectFlags flags) const {
	for(auto cell : grid.trace(seg))
		for(auto eid : grid.cellEdges(cell)) {
			if(flags & (*this)[eid].classifyIsect(seg))
				return true;
		}
	return false;
}

template <class T>
template <class U, EnableIfFptVec<U>...>
Maybe<VertexId> PlaneGraph<T>::closestVertex(const Point &point, Scalar max_dist) const {
	DASSERT(m_grid);
	return m_grid->closestVertex(point, m_points, max_dist);
}

template <class T> vector<EdgeId> PlaneGraph<T>::findIntersectors(const Grid &grid) const {
	vector<EdgeId> out;

	for(auto cell_id : grid) {
		if(!grid.empty(cell_id)) {
			auto edges = grid.cellEdges(cell_id);
			auto verts = grid.cellNodes(cell_id);

			for(int e1 : intRange(edges)) {
				EdgeRef edge1(ref(edges[e1]));
				Segment seg1 = (*this)[edge1];
				bool is_isecting = false;

				for(int e2 : intRange(e1 + 1, edges.size())) {
					EdgeId edge2(edges[e2]);
					if(!edge1.adjacent(edge2)) {
						Segment seg2 = (*this)[edge2];
						if(seg1.classifyIsect(seg2) != IsectClass::none) {
							is_isecting = true;
							out.push_back(edge2);
						}
					}
				}

				if(!is_isecting)
					for(VertexId node : verts) {
						if(!edge1.adjacent(node)) {
							if(seg1.classifyIsect((*this)[node]) != IsectClass::none) {
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

template <class T>
vector<Variant<VertexId, EdgeId>> PlaneGraph<T>::findIntersectors(const Grid &grid,
																  double min_dist) const {
	vector<Variant<VertexId, EdgeId>> out;

	vector<bool> edge_map(numEdges());
	for(auto eid : findIntersectors(grid)) {
		edge_map[eid] = true;
		out.emplace_back(eid);
	}

	double min_dist_sq = min_dist * min_dist;

	// TODO: optimize this
	for(EdgeRef eref : edgeRefs()) {
		Segment seg((*this)[eref]);

		for(auto nid : vertexIds()) {
			if(eref.adjacent(nid))
				continue;
			auto dist_sq = seg.distanceSq(m_points[nid]);
			if(double(dist_sq) < min_dist_sq) {
				out.emplace_back(nid);
				if(!edge_map[eref]) {
					edge_map[eref] = true;
					out.emplace_back((EdgeId)eref);
				}
			}
		}
	}

	return out;
}

template <class T> Expected<void> PlaneGraph<T>::checkPlanar() const {
	DASSERT(m_grid);
	auto isectors = findIntersectors(*m_grid);
	if(isectors) {
		/* // TODO: enable geom debugging hooks
		auto bad_segs = fwk::transform(isectors, [this](auto edge_id) { return (*this)[edge_id]; });
		using namespace vis;
		Visualizer2 vis;
		vis(segments());
		vis(m_points);
		vis(bad_segs, ColorId::red);*/

		return Error("Graph not planar"); // << vis;
	}
	return {};
}

template <class T> Expected<void> PlaneGraph<T>::checkPlanar(double min_dist) const {
	DASSERT(m_grid);
	auto isectors = findIntersectors(*m_grid, min_dist);
	if(isectors) {
		vector<Segment> bad_segs;
		vector<double2> bad_points;
		for(auto isect : isectors) {
			if(const EdgeId *eid = isect)
				bad_segs.emplace_back(Segment((*this)[*eid]));
			if(const VertexId *nid = isect)
				bad_points.emplace_back((*this)[*nid]);
		}

		/* // TODO: enable geom debugging hooks
		using namespace vis;
		VisFunc2 vis_func = [=](Visualizer2 &vis, double2 mouse_pos) -> string {
			vis(segments());
			vis(m_points);
			vis(bad_segs, ColorId::red);
			vis(bad_points, ColorId::red);
			return "";
		};*/

		return Error(format("Graph not planar (tolerance: %)", min_dist)); // << move(vis_func);
	}
	return {};
}

template <class T> bool PlaneGraph<T>::isPlanar(const Grid &grid) const {
	auto isectors = findIntersectors(grid);

	/* // TODO: enable geom debugging hooks
	if(visDebugging() && !isectors.empty()) {
		auto bad_segs = fwk::transform(isectors, [this](auto edge_id) { return (*this)[edge_id]; });

		auto vis_func = [&](Visualizer2 &vis, double2 mouse_pos) -> string {
			vis(segments());
			vis(points());
			vis(bad_segs, ColorId::red);
			return "Graph not planar";
		};
		investigate(vis_func, (DRect)encloseRange(bad_segs));
	}*/

	return isectors.empty();
}

template <class T> bool PlaneGraph<T>::isPlanar() const {
	return isPlanar(hasGrid() ? *m_grid : makeGrid());
}

template <class T>
template <class U, EnableIfFptVec<U>...>
vector<T> PlaneGraph<T>::evenPoints(Scalar dist) const {
	Builder builder;

	for(const auto &contour : contours()) {
		double len = contour.length();
		int num_points = std::floor(len / dist);
		double step = len / (num_points + 1);

		builder((T)contour.points().front());
		for(int n : intRange(num_points))
			builder(T(contour.point(double(n + 1) * step)));
		if(!contour.isLooped())
			builder((T)contour.points().back());
	}

	return builder.extractPoints();
}

template <class T>
template <class U, EnableIfFptVec<U>...>
PGraph<T> PlaneGraph<T>::simplify(Scalar theta, Scalar max_err, Scalar max_dist) const {
	return fromContours(
		fwk::transform(contours(), [=](auto &c) { return c.simplify(theta, max_err, max_dist); }));
}

template <class T>
template <class U, EnableIfFptVec<U>...>
PlaneGraph<T> PlaneGraph<T>::splitEdges(Scalar max_len) const {
	Builder builder(m_points);
	builder.reserveEdges(numEdges());

	auto max_len_sq = max_len * max_len;
	vector<T> points;
	points.reserve(32);

	vector<bool> visited(numEdges(), false);

	for(auto eref : edgeRefs()) {
		if(visited[eref])
			continue;

		auto seg = (*this)[eref];
		if(seg.lengthSq() > max_len_sq) {
			auto len = seg.length();
			int num_points = std::floor(len / max_len);
			auto step = 1.0 / (num_points + 1);

			points.clear();
			points.emplace_back(seg.from);
			for(int n : intRange(num_points))
				points.emplace_back(seg.at(Scalar(n + 1) * step));
			points.emplace_back(seg.to);

			for(int n : intRange(0, points.size() - 1))
				builder(points[n], points[n + 1]);

			if(eref.twin()) {
				for(int n = points.size() - 1; n > 0; n--)
					builder(points[n], points[n - 1]);
				visited[*eref.twin()] = true;
			}
		} else {
			builder(seg.from, seg.to);
			if(eref.twin()) {
				builder(seg.to, seg.from);
				visited[*eref.twin()] = true;
			}
		}
	}

	return builder.build();
}

template <class T>
PlaneGraph<T> PlaneGraph<T>::merge(CSpan<PlaneGraph<T>> graphs,
								   vector<Pair<VertexId>> *vert_intervals) {
	Builder builder;

	if(vert_intervals) {
		vert_intervals->clear();
		vert_intervals->reserve(graphs.size());
	}

	// TODO: assert that there are no edge-duplicates
	for(auto &graph : graphs) {
		VertexId nmin = no_init, nmax = no_init;
		if(graph.numVerts() != 0)
			nmin = nmax = builder(graph[VertexId(0)]);

		for(auto vref : graph.vertexRefs()) {
			auto id = builder(graph[vref]);
			nmin = min(nmin, id);
			nmax = max(nmax, id);
		}

		if(vert_intervals)
			vert_intervals->emplace_back(nmin, nmax);

		// TODO: how to handle duplicates ?
		for(auto eref : graph.edgeRefs())
			builder(graph[eref.from()], graph[eref.to()]);
	}

	return builder.build();
}

template <class T> auto bestIntegralScale_(CSpan<T> points, int max_value) {
	auto rect = enclose(points);
	auto tvec = vmax(vabs(rect.min()), vabs(rect.max()));
	auto tmax = max(tvec[0], tvec[1]);
	if(T::vec_size >= 3)
		tmax = max(tmax, tvec[2]);
	if(tmax < 1.0)
		tmax = 1.0;
	return double(max_value) / tmax;
}

double bestIntegralScale(CSpan<double2> points, int max_value) {
	return bestIntegralScale_(points, max_value);
}
double bestIntegralScale(CSpan<float2> points, int max_value) {
	return bestIntegralScale_(points, max_value);
}
double bestIntegralScale(CSpan<float3> points, int max_value) {
	return bestIntegralScale_(points, max_value);
}
double bestIntegralScale(CSpan<double3> points, int max_value) {
	return bestIntegralScale_(points, max_value);
}

template <class IT, class T, EnableIfFptVec<T, 2>..., EnableIfIntegralVec<IT, 2>...>
Maybe<vector<IT>> tryToIntegral(CSpan<T> points, double scale) {
	PGraphBuilder<IT> builder;
	builder.reservePoints(points.size());

	for(auto pt : points)
		builder(IT(pt * scale));
	if(builder.numVerts() == points.size())
		return builder.extractPoints();
	return none;
}

static void removeEmptyEdges(vector<Pair<VertexId>> &edges) {
	edges = filter(edges, [](auto &pair) { return pair.first != pair.second; });
}

template <class T, EnableIfVec<T, 2>...>
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

template <class IT, class T, EnableIfFptVec<T, 2>..., EnableIfIntegralVec<IT, 2>...>
PGraph<IT> toIntegral(const PGraph<T> &graph, double scale, bool remove_collapsed_edges) {
	PGraphBuilder<IT> builder;
	builder.reservePoints(graph.numVerts());

	vector<VertexId> mapping;
	mapping.reserve(graph.numVerts());

	for(auto pt : graph.points())
		mapping.emplace_back(builder(IT(pt * scale)));

	// TODO: use point builder ?
	auto edges = remapVerts(graph.edgePairs(), mapping);
	if(remove_collapsed_edges)
		removeEmptyEdges(edges);
	makeSortedUnique(edges);
	return PGraph<IT>{edges, builder.extractPoints()};
}

template <class T>
template <class U, EnableIfFptVec<U>...>
vector<T> PlaneGraph<T>::randomPoints(Random &random, Scalar min_dist, Maybe<Rect> trect) const {
	DASSERT(m_grid);
	DASSERT_GT(min_dist, Scalar(0));

	auto rect = trect ? *trect : enclose(m_points);
	RegularGrid<Vec> ugrid(rect, Scalar(min_dist / std::sqrt(2.0)), 1);

	auto min_dist_sq = min_dist * min_dist;

	T invalid_pt(inf);
	vector<T> points(ugrid.width() * ugrid.height(), invalid_pt);

	vector<T> out;
	out.reserve(ugrid.width() * ugrid.height() / 4);

	auto segs = segments();

	for(auto pos : cells(ugrid.cellRect().inset(1))) {
		auto pt = ugrid.toWorld(pos) + random.sampleBox(T(), T(1, 1)) * ugrid.cellSize();

		int idx = pos.x + pos.y * ugrid.width();
		int indices[4] = {idx - 1, idx - ugrid.width(), idx - ugrid.width() - 1,
						  idx - ugrid.width() + 1};

		if(allOf(indices, [&](int idx) { return distanceSq(points[idx], pt) >= min_dist_sq; }))
			if(!m_grid->closestEdge(pt, segs, min_dist)) {
				points[idx] = pt;
				out.emplace_back(pt);
			}
	}

	return out;
}

template <class T>
template <class U, EnableIfFptVec<U>...>
auto PlaneGraph<T>::joinNearby(Scalar join_dist) const -> PointTransform {
	// When computing on integers, you have to make sure not to overflow
	// when computing distance

	auto rect = enclose(m_points);

	auto points = m_points;

	auto join_dist_sq = join_dist * join_dist;
	auto cell_size = join_dist;

	RegularGrid<T> grid(rect, cell_size);
	using IndexVec = SmallVector<int, smallVectorSize<int>(32)>;
	HashMap<int2, IndexVec> map;

	for(int id : intRange(points)) {
		auto cell_pos = grid.toCell(points[id]);
		map[cell_pos].emplace_back(id);
	}

	vector<Scalar> weights(points.size(), Scalar(1));
	bool something_changed = true;

	vector<int> new_index;
	new_index.reserve(points.size());
	int num_points = points.size();

	for(int i : intRange(points))
		new_index.emplace_back(i);

	auto merge_into = [&](int id1) -> bool {
		auto cell_pos = grid.toCell(points[id1]);
		bool did_merge = false;

		for(auto npos : grid.nearby9(cell_pos)) {
			auto it = map.find(npos);
			if(it == map.end())
				continue;

			auto &value_vec = it->second;
			for(int i2 = 0; i2 < value_vec.size(); i2++) {
				auto id2 = value_vec[i2];
				if(id2 == id1)
					continue;

				auto dist_sq = distanceSq(points[id1], points[id2]);
				if(dist_sq < join_dist_sq) {
					auto new_weight = weights[id1] + weights[id2];
					auto new_point = (points[id1] * weights[id1] + points[id2] * weights[id2]);
					new_point /= new_weight;

					//	print("merging: % | %  (% -> %), dist: %\n", points[id1], points[id2], id2,
					//			 id1, std::sqrt(dist_sq));
					weights[id1] = new_weight;
					weights[id2] = Scalar(0);
					points[id1] = new_point;
					new_index[id2] = id1;
					value_vec[i2--] = value_vec.back();
					value_vec.pop_back();
					num_points--;
					did_merge = true;
				}
			}
		}

		return did_merge;
	};

	for(int id1 : intRange(points)) {
		if(new_index[id1] != id1)
			continue;
		bool needs_merge = true;

		while(needs_merge) {
			auto old_pos = grid.toCell(points[id1]);
			bool did_merge = merge_into(id1);
			needs_merge = false;

			if(did_merge) {
				auto new_pos = grid.toCell(points[id1]);
				if(old_pos != new_pos) {
					auto &vec = map[old_pos];
					for(int i : intRange(vec))
						if(vec[i] == id1) {
							vec[i] = vec.back();
							vec.pop_back();
							break;
						}
					map[new_pos].emplace_back(id1);
				}
				needs_merge = true;
			}
		}
	}

	print("Merged % points out of: %\n", points.size() - num_points, points.size());

	PointTransform out;
	out.new_points.reserve(num_points);

	for(int i : intRange(points)) {
		if(new_index[i] == i) {
			auto pt = points[new_index[i]];
			new_index[i] = out.new_points.size();
			out.new_points.emplace_back(pt);
		} else {
			new_index[i] = -new_index[i] - 1;
		}
	}
	for(int i : intRange(points))
		if(new_index[i] < 0)
			new_index[i] = new_index[-new_index[i] - 1];
	out.mapping = new_index.reinterpret<VertexId>();
	return out;
}

template <class T> auto PlaneGraph<T>::tied() const {
	return fwk::tie((const ImmutableGraph &)*this, m_points);
}

template <class T> bool PlaneGraph<T>::operator==(const PlaneGraph &rhs) const {
	return tied() == rhs.tied();
}
template <class T> bool PlaneGraph<T>::operator<(const PlaneGraph &rhs) const {
	return tied() < rhs.tied();
}

#define INSTANTIATE(T)                                                                             \
	template class PlaneGraph<T>;                                                                  \
	template void orderByDirection(Span<int>, CSpan<T>, const T &);

#define INSTANTIATE_FP(T)                                                                          \
	INSTANTIATE(T)                                                                                 \
	template vector<Contour<T>> PlaneGraph<T>::contours() const;                                   \
	template vector<Contour<T>> PlaneGraph<T>::contourLoops() const;                               \
	template PlaneGraph<T> PlaneGraph<T>::simplify(Scalar, Scalar, Scalar) const;                  \
	template PlaneGraph<T> PlaneGraph<T>::splitEdges(Scalar max_length) const;                     \
	template vector<T> PlaneGraph<T>::evenPoints(Scalar dist) const;                               \
	template auto PlaneGraph<T>::joinNearby(Scalar join_dist) const->PointTransform;               \
	template PlaneGraph<T> PlaneGraph<T>::fromContours(CSpan<Contour<T>>);                         \
	template PlaneGraph<T> PlaneGraph<T>::fromSegments(CSpan<Segment>);                            \
	template Maybe<VertexId> PlaneGraph<T>::closestVertex(const Point &, Scalar max_dist) const;   \
	template vector<T> PlaneGraph<T>::randomPoints(Random &, Scalar, Maybe<Rect>) const;           \
	template Maybe<vector<int2>> tryToIntegral(CSpan<T>, double);                                  \
	template PGraph<int2> toIntegral(const PGraph<T> &, double, bool);                             \
	template Maybe<vector<short2>> tryToIntegral(CSpan<T>, double);                                \
	template PGraph<short2> toIntegral(const PGraph<T> &, double, bool);

INSTANTIATE(int2)
INSTANTIATE(short2)
INSTANTIATE_FP(float2)
INSTANTIATE_FP(double2)
}
