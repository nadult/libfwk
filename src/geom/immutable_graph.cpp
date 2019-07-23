// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/immutable_graph.h"

#include "fwk/algorithm.h"
#include "fwk/heap.h"
#include "fwk/math/constants.h"
#include "fwk/sys/assert.h"

namespace fwk {
using EdgeRef = ImmutableGraph::EdgeRef;
using VertexRef = ImmutableGraph::VertexRef;

ImmutableGraph::ImmutableGraph(CSpan<Pair<VertexId>> edges, Maybe<int> num_verts) {
	if(!num_verts) {
		int max_node = -1;
		for(auto &edge : edges)
			max_node = max(max_node, (int)edge.first, (int)edge.second);
		num_verts = max_node + 1;
	}

	m_vert_info.resize(*num_verts);
	m_edge_info.reserve(edges.size());
	for(auto edge : edges) {
		DASSERT_EX(valid(edge.first), edge.first);
		DASSERT_EX(valid(edge.second), edge.second);
		m_vert_info[edge.first].num_edges_from++;
		m_vert_info[edge.second].num_edges_to++;
		m_edge_info.emplace_back(EdgeInfo{edge.first, edge.second});
	}

	int edge_offset = 0;
	vector<pair<int, int>> eoffsets;
	eoffsets.reserve(m_vert_info.size());

	for(auto &ninfo : m_vert_info) {
		ninfo.first_edge = edge_offset;
		eoffsets.emplace_back(edge_offset, edge_offset + ninfo.num_edges_from);
		edge_offset += ninfo.num_edges_from + ninfo.num_edges_to;
	}
	m_incidence_info.resize(edge_offset, no_init);

	for(int eid = 0; eid < edges.size(); eid++) {
		const auto &edge = edges[eid];
		int from_pos = eoffsets[edge.first].first++;
		int to_pos = eoffsets[edge.second].second++;
		m_incidence_info[from_pos] = EdgeId(eid);
		m_incidence_info[to_pos] = EdgeId(eid);
	}

	DASSERT(!hasEdgeDuplicates());
}

void ImmutableGraph::computeExtendedInfo() {
	m_ext_info.resize(m_edge_info.size());
	for(auto nid : vertexIds()) {
		auto edges = edgesFrom(nid);
		if(!edges)
			continue;

		EdgeId prev = edges.back();
		for(auto eid : edges) {
			m_ext_info[prev].next_from = eid;
			m_ext_info[eid].prev_from = prev;
			prev = eid;
		}

		edges = edgesTo(nid);
		if(!edges)
			continue;

		prev = edges.back();
		for(auto eid : edges) {
			m_ext_info[prev].next_to = eid;
			m_ext_info[eid].prev_to = prev;
			prev = eid;
		}
	}
}

bool ImmutableGraph::hasEdgeDuplicates() const {
	for(auto eref1 : edgeRefs()) {
		for(auto eref2 : eref1.from().edgesFrom())
			if(eref2 != eref1 && eref2.to() == eref1.to()) {
				print("duplicate: %(% %): %(% %)\n", eref1, eref1.from(), eref1.to(), eref2,
					  eref2.from(), eref2.to());
				return true;
			}
	}
	return false;
}

ImmutableGraph ImmutableGraph::makeForest(CSpan<Maybe<VertexId>> parents, Maybe<int> num_verts) {
	vector<pair<VertexId, VertexId>> edges;
	edges.reserve(parents.size());
	for(auto vert_id : indexRange<VertexId>(parents))
		if(parents[vert_id])
			edges.emplace_back(*parents[vert_id], vert_id);
	return {edges, num_verts};
}

Maybe<EdgeRef> ImmutableGraph::findEdge(VertexId from, VertexId to) const {
	for(auto edge_id : edgesFrom(from)) {
		const auto &edge = m_edge_info[edge_id];
		if(edge.second == to)
			return EdgeRef(*this, edge_id);
	}
	return none;
}

CSpan<EdgeId> ImmutableGraph::edgesFrom(VertexId vert_id) const {
	DASSERT(valid(vert_id));
	auto &node = m_vert_info[vert_id];
	return {m_incidence_info.data() + node.first_edge, node.num_edges_from};
}

CSpan<EdgeId> ImmutableGraph::edgesTo(VertexId vert_id) const {
	DASSERT(valid(vert_id));
	auto &node = m_vert_info[vert_id];
	return {m_incidence_info.data() + (node.first_edge + node.num_edges_from), node.num_edges_to};
}

CSpan<EdgeId> ImmutableGraph::edges(VertexId vert_id) const {
	DASSERT(valid(vert_id));
	auto &node = m_vert_info[vert_id];
	return {m_incidence_info.data() + node.first_edge, node.num_edges_to + node.num_edges_from};
}

vector<VertexId> ImmutableGraph::vertsFrom(VertexId vert_id) const {
	return transform(edgesFrom(vert_id), [&](auto edge_id) { return m_edge_info[edge_id].second; });
}

vector<VertexId> ImmutableGraph::vertsTo(VertexId vert_id) const {
	return transform(edgesTo(vert_id), [&](auto edge_id) { return m_edge_info[edge_id].first; });
}

vector<VertexId> ImmutableGraph::vertsAdj(VertexId vert_id) const {
	auto out = transform(edges(vert_id), [&](auto edge_id) {
		auto &edge = m_edge_info[edge_id];
		return edge.first == vert_id ? edge.second : edge.first;
	});
	makeSortedUnique(out);
	return out;
}

VertexId ImmutableGraph::from(EdgeId edge_id) const {
	DASSERT(valid(edge_id));
	return m_edge_info[edge_id].first;
}

VertexId ImmutableGraph::to(EdgeId edge_id) const {
	DASSERT(valid(edge_id));
	return m_edge_info[edge_id].second;
}

// TODO: this is wrong
vector<array<VertexId, 3>> ImmutableGraph::triangles() const {
	vector<VertexId> buffer1, buffer2;
	buffer1.reserve(32);
	buffer2.reserve(32);
	vector<bool> visited(numEdges(), false);

	vector<array<VertexId, 3>> out;

	for(auto eref : edgeRefs()) {
		auto n1 = eref.from(), n2 = eref.to();
		buffer1.clear();
		buffer2.clear();

		for(auto eref1 : n1.edges())
			if(!visited[eref1])
				buffer1.push_back(eref1.other(n1));
		for(auto eref2 : n2.edges())
			if(!visited[eref2])
				buffer2.push_back(eref2.other(n2));
		for(auto i1 : buffer1)
			for(auto i2 : buffer2)
				if(i1 == i2)
					out.push_back(array<VertexId, 3>{{n1, n2, i1}});

		visited[eref] = true;
		if(auto twin = eref.twin())
			visited[*twin] = true;
	}

	return out;
}

ImmutableGraph ImmutableGraph::asUndirected() const {
	vector<pair<VertexId, VertexId>> out = edgePairs();
	for(auto eref : edgeRefs())
		if(!eref.twin())
			out.emplace_back(eref.to().id(), eref.from().id());
	return {out, numVerts()};
}

bool ImmutableGraph::isUndirected() const {
	return allOf(edgeRefs(), [](auto eref) { return !!eref.twin(); });
}

vector<pair<VertexId, VertexId>> remapVerts(CSpan<pair<VertexId, VertexId>> edges,
											CSpan<VertexId> map) {
	return transform(edges, [=](auto edge) -> pair<VertexId, VertexId> {
		return {map[edge.first], map[edge.second]};
	});
}

ImmutableGraph remapVerts(const ImmutableGraph &graph, CSpan<VertexId> map) {
	DASSERT_EQ(map.size(), graph.numVerts());
	if(map.empty())
		return graph;
	auto max_node = (int)*std::min_element(begin(map), end(map));
	return ImmutableGraph(remapVerts(graph.edgePairs(), map), max_node + 1);
}

template <class T, EnableIf<is_scalar<T>>...>
ImmutableGraph ImmutableGraph::minimumSpanningTree(CSpan<T> edge_weights,
												   bool as_undirected) const {
	DASSERT(edge_weights.size() == numEdges());

	if(numVerts() == 0)
		return {};

	vector<EdgeId> out;
	Heap<T> heap(numVerts());

	vector<bool> processed(numVerts(), false);
	vector<Maybe<VertexId>> pi(numVerts());
	vector<T> keys(pi.size(), inf);
	keys[vertexIds()[0]] = T(0);

	for(auto node : vertexIds())
		heap.insert(node, keys[node]);

	while(!heap.empty()) {
		auto nref = ref(VertexId(heap.extractMin().second));
		processed[nref] = true;

		auto edges = as_undirected ? nref.edges() : nref.edgesFrom();
		for(auto eref : edges) {
			auto nnode = eref.other(nref);
			auto weight = edge_weights[eref];

			if(!processed[nnode] && weight < keys[nnode]) {
				pi[nnode] = nref.id();
				keys[nnode] = weight;
				heap.update(nnode, weight);
			}
		}
	}

	return makeForest(pi, numVerts());
}

ImmutableGraph ImmutableGraph::shortestPathTree(CSpan<VertexId> sources,
												CSpan<double> weights) const {
	Heap<double> heap(numVerts());
	vector<double> keys(numVerts(), inf);

	if(!weights.empty()) {
		DASSERT_EQ(weights.size(), numEdges());
		for(auto weight : weights)
			DASSERT_GE(weight, 0.0);
	}

	for(auto src_id : sources)
		keys[src_id] = 0.0;
	for(auto vert_id : vertexIds())
		heap.insert(vert_id, keys[vert_id]);
	vector<bool> visited(numVerts(), false);

	vector<Maybe<VertexId>> out(numVerts());

	while(!heap.empty()) {
		auto nref = ref(VertexId(heap.extractMin().second));
		visited[nref] = true;

		for(auto eref : nref.edgesFrom()) {
			auto target = eref.to();
			if(visited[target])
				continue;

			auto new_key = (weights.empty() ? 1.0 : weights[eref]) + keys[nref];
			if(new_key < keys[target]) {
				out[target] = nref;
				keys[target] = new_key;
				heap.update(target, new_key);
			}
		}
	}

	return makeForest(out, numVerts());
}

ImmutableGraph ImmutableGraph::reversed() const {
	auto swap_pair = [](auto &p) { return pair{p.second, p.first}; };
	return {fwk::transform(edgePairs(), swap_pair), numVerts()};
}

bool ImmutableGraph::hasCycles() const {
	enum Mode { exit, enter };
	vector<pair<VertexId, Mode>> stack;
	stack.reserve(numVerts());

	enum Status { not_visited, visiting, visited };
	vector<Status> status(numVerts(), not_visited);

	for(auto start_id : vertexIds()) {
		if(status[start_id] != not_visited)
			continue;

		stack.emplace_back(start_id, enter);

		while(!stack.empty()) {
			auto vert_id = stack.back().first;
			auto mode = stack.back().second;

			if(mode == enter) {
				if(status[vert_id] != not_visited) {
					stack.pop_back();
					continue;
				}

				status[vert_id] = visiting;

				stack.back().second = exit;
				for(auto edge_id : edgesFrom(vert_id)) {
					auto next_id = to(edge_id);
					if(status[next_id] == visiting)
						return true;
					if(status[next_id] == not_visited)
						stack.emplace_back(next_id, enter);
				}
			} else {
				status[vert_id] = visited;
				stack.pop_back();
			}
		}
	}

	return false;
}

bool ImmutableGraph::isForest() const {
	if(hasCycles()) {
		print("cycle!\n");
		return false;
	}

	for(auto nref : vertexRefs())
		if(nref.numEdgesTo() > 1)
			return false;
	return true;
}

vector<VertexId> ImmutableGraph::treeRoots() const {
	auto pred = [this](VertexId nid) { return ref(nid).numEdgesTo() == 0; };
	return filter(vertexIds(), pred, countIf(vertexIds(), pred));
}

FWK_ORDER_BY_DEF(ImmutableGraph)
FWK_ORDER_BY_DEF(ImmutableGraph::VertexInfo)

template ImmutableGraph ImmutableGraph::minimumSpanningTree(CSpan<float>, bool) const;
template ImmutableGraph ImmutableGraph::minimumSpanningTree(CSpan<double>, bool) const;
}
