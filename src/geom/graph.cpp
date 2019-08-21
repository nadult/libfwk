// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/graph.h"

#include "fwk/heap.h"
#include "fwk/index_range.h"
#include "fwk/math/constants.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/memory.h"

namespace fwk {

template <class T> using FixedElem = Graph::FixedElem<T>;

using Label = GLabel;
using Layer = GLayer;
using Layers = GLayers;

Graph::Graph() = default;
FWK_COPYABLE_CLASS_IMPL(Graph);

Graph::Graph(CSpan<Pair<VertexId>> edges, Maybe<int> num_verts) {
	if(!num_verts) {
		int vmax = 0;
		for(auto [v1, v2] : edges)
			vmax = max(vmax, (int)v1, (int)v2);
		num_verts = vmax + 1;
	}
	m_verts.reserve(*num_verts);
	for(int n : intRange(*num_verts))
		addVertex();
	m_edges.reserve(edges.size());
	for(auto [v1, v2] : edges)
		addEdge(v1, v2);
}

CSpan<Pair<VertexId>> Graph::indexedEdges() const {
	using Edges = decltype(m_edges);
	static_assert(Edges::compatible_alignment && Edges::same_size);
	static_assert(sizeof(EdgeInfo) == sizeof(Pair<VertexId>));
	return {reinterpret_cast<const Pair<VertexId> *>(m_edges.rawData()), m_edges.endIndex()};
}

CSpan<Graph::VertexInfo> Graph::indexedVerts() const {
	using Verts = decltype(m_verts);
	static_assert(Verts::compatible_alignment && Verts::same_size);
	return {m_verts.rawData(), m_verts.endIndex()};
}

int Graph::numVerts(Layers layers) const {
	int count = 0;
	for(auto id : m_verts.indices())
		if(m_vert_layers[id] & layers)
			count++;
	return count;
}

int Graph::numEdges(Layers layers) const {
	int count = 0;
	for(auto id : m_edges.indices())
		if(m_edge_layers[id] & layers)
			count++;
	return count;
}
int Graph::numTris(Layers layers) const {
	int count = 0;
	for(auto id : m_tris.indices())
		if(m_tri_layers[id] & layers)
			count++;
	return count;
}

// -------------------------------------------------------------------------------------------
// ---  Access to graph elements -------------------------------------------------------------

vector<VertexId> Graph::vertexIds(Layers layer_mask) const {
	Vector<VertexId> out;
	for(auto idx : m_verts.indices()) {
		if(layer_mask == all<Layer> || (m_vert_layers[idx] & layer_mask))
			out.emplace_back(idx);
	}
	return out;
}

vector<EdgeId> Graph::edgeIds(Layers layer_mask) const {
	Vector<EdgeId> out;
	for(auto idx : m_edges.indices())
		if(layer_mask == all<Layer> || (m_edge_layers[idx] & layer_mask))
			out.emplace_back(idx);
	return out;
}

Maybe<EdgeRef> Graph::findEdge(VertexId from, VertexId to, Layers layers) const {
	for(auto eid : m_verts[from])
		if(eid.isSource() && eid.test(layers) && m_edges[eid].to == to)
			return EdgeRef{this, eid};
	return none;
}

Maybe<EdgeRef> Graph::findUndirectedEdge(VertexId from, VertexId to, Layers layers) const {
	for(auto eid : m_verts[from])
		if(eid.isSource() && eid.test(layers) && m_edges[eid].to == to)
			return EdgeRef{this, eid};
	for(auto eid : m_verts[to])
		if(eid.isSource() && eid.test(layers) && m_edges[eid].to == from)
			return EdgeRef{this, eid};
	return none;
}

Maybe<TriId> Graph::findTri(VertexId v1, VertexId v2, VertexId v3, Layers layers) const {
	auto vmax = max(v1, v2, v3);
	if(vmax >= m_vert_tris.size())
		return none;

	for(auto tid : m_vert_tris[v1]) {
		auto &tri = m_tris[tid];
		if(isOneOf(v2, tri.verts) && isOneOf(v3, tri.verts))
			return TriId(tid);
	}
	return none;
}

/*
vector<VertexId> Graph::nodesAdj(VertexId node_id) const {
	auto out = transform(edges(node_id), [&](auto edge_id) {
		auto &edge = m_edges[edge_id];
		return edge.from == node_id ? edge.to : edge.from;
	});
	makeSortedUnique(out);
	return out;
}*/

VertexId Graph::from(EdgeId edge_id) const { return m_edges[edge_id].from; }
VertexId Graph::to(EdgeId edge_id) const { return m_edges[edge_id].to; }

static const Label g_default_label;

bool Graph::hasLabel(VertexId id) const { return m_vert_labels.find(id) == m_vert_labels.end(); }
bool Graph::hasLabel(EdgeId id) const { return m_edge_labels.find(id) == m_edge_labels.end(); }

const Label &Graph::operator[](VertexId vid) const {
	auto it = m_vert_labels.find(vid);
	return it == m_vert_labels.end() ? g_default_label : it->second;
}

const Label &Graph::operator[](EdgeId eid) const {
	auto it = m_edge_labels.find(eid);
	return it == m_edge_labels.end() ? g_default_label : it->second;
}

const Label &Graph::operator[](TriId tid) const {
	auto it = m_tri_labels.find(tid);
	return it == m_tri_labels.end() ? g_default_label : it->second;
}

Label &Graph::operator[](VertexId vid) { return m_vert_labels[vid]; }
Label &Graph::operator[](EdgeId eid) { return m_edge_labels[eid]; }
Label &Graph::operator[](TriId tid) { return m_tri_labels[tid]; }

Layers Graph::layers(VertexId id) const {
	PASSERT(valid(id));
	return m_vert_layers[id];
}

Layer Graph::layer(EdgeId id) const {
	PASSERT(valid(id));
	return m_edge_layers[id];
}

Layer Graph::layer(TriId id) const {
	PASSERT(valid(id));
	return m_tri_layers[id];
}

// -------------------------------------------------------------------------------------------
// ---  Adding & removing elements -----------------------------------------------------------

void Graph::clear() {
	m_verts.clear();
	m_edges.clear();
	m_tris.clear();
	m_vert_tris.clear();

	m_vert_labels.clear();
	m_edge_labels.clear();
	m_tri_labels.clear();
}

void Graph::reserveVerts(int capacity) { m_verts.reserve(capacity); }
void Graph::reserveEdges(int capacity) { m_edges.reserve(capacity); }
void Graph::reserveTris(int capacity) { m_tris.reserve(capacity); }

VertexId Graph::addVertex(Layers layers) {
	auto vid = VertexId(m_verts.emplace());
	m_vert_layers.resize(m_verts.capacity());
	m_vert_layers[vid] = layers;
	return vid;
}

void Graph::addVertexAt(VertexId vid, Layers layers) {
	DASSERT(!m_verts.valid(vid));
	m_verts.emplaceAt(vid);
	m_vert_layers.resize(m_verts.capacity());
	m_vert_layers[vid] = layers;
}

EdgeId Graph::addEdge(VertexId v1, VertexId v2, Layer layer) {
	DASSERT(valid(v1) && valid(v2));
	DASSERT(v1 != v2);

	EdgeId eid(m_edges.emplace(v1, v2));
	m_edge_layers.resize(m_edges.capacity());
	m_edge_layers[eid] = layer;
	m_verts[v1].emplace_back(eid.index(), layer, true);
	m_verts[v2].emplace_back(eid, layer, false);
	return eid;
}

void Graph::addEdgeAt(EdgeId eid, VertexId v1, VertexId v2, Layer layer) {
	DASSERT_EX(valid(v1) && valid(v2), v1, v2);
	DASSERT(v1 != v2);
	DASSERT(!valid(eid));

	m_edges.emplaceAt(eid, v1, v2);
	m_edge_layers.resize(m_edges.capacity());
	m_edge_layers[eid] = layer;
	m_verts[v1].emplace_back(eid.index(), layer, true);
	m_verts[v2].emplace_back(eid, layer, false);
}

FixedElem<EdgeId> Graph::fixEdge(VertexId v1, VertexId v2, Layer layer) {
	if(auto id = findEdge(v1, v2, layer))
		return {*id, false};
	return {addEdge(v1, v2, layer), true};
}

FixedElem<EdgeId> Graph::fixUndirectedEdge(VertexId v1, VertexId v2, Layer layer) {
	if(auto id = findUndirectedEdge(v1, v2, layer))
		return {*id, false};
	return {addEdge(v1, v2, layer), true};
}

TriId Graph::addTri(VertexId v1, VertexId v2, VertexId v3, Layer layer) {
	PASSERT(valid(v1) && valid(v2) && valid(v3));
	DASSERT(v1 != v2 && v2 != v3 && v3 != v1);

	TriId tid(m_tris.emplace(v1, v2, v3));
	m_tri_layers.resize(m_tris.capacity());
	m_tri_layers[tid] = layer;

	auto vmax = max(v1, v2, v3);
	if(m_vert_tris.size() <= vmax)
		m_vert_tris.resize(int(vmax) + 1);

	m_vert_tris[v1].emplace_back(tid, layer, 0);
	m_vert_tris[v2].emplace_back(tid, layer, 1);
	m_vert_tris[v3].emplace_back(tid, layer, 2);
	return tid;
}

FixedElem<TriId> Graph::fixTri(VertexId v1, VertexId v2, VertexId v3, Layer layer) {
	if(auto id = findTri(v1, v2, v3, layer))
		return {*id, false};
	return {addTri(v1, v2, v3, layer), true};
}

void Graph::remove(VertexId vid) {
	for(auto eid : m_verts[vid])
		remove(eid);
	if(m_vert_tris.size() > vid)
		for(auto tid : m_vert_tris[vid])
			remove(tid);
	m_verts.erase(vid);
	m_vert_labels.erase(vid);
}

void Graph::remove(EdgeId eid) {
	auto &edge = m_edges[eid];
	for(auto nid : {edge.from, edge.to}) {
		auto &node = m_verts[nid];
		for(int n = 0; n < node.size(); n++)
			if(node[n] == eid) {
				while(n + 1 < node.size()) {
					node[n] = node[n + 1];
					n++;
				}
				break;
			}
	}
	m_edges.erase(eid);
	m_edge_labels.erase(eid);
}

void Graph::remove(TriId tid) {
	for(auto nid : m_tris[tid].verts) {
		auto &vtris = m_vert_tris[nid];
		for(int n : intRange(vtris))
			if(vtris[n] == tid) {
				vtris[n] = vtris.back();
				vtris.pop_back();
				break;
			}
	}
	m_tris.erase(tid);
	m_tri_labels.erase(tid);
}

// -------------------------------------------------------------------------------------------
// ---  Algorithms ---------------------------------------------------------------------------

bool Graph::hasEdgeDuplicates() const {
	vector<bool> bitmap(m_verts.size(), false);

	for(auto n1 : vertexIds()) {
		for(auto e1 : m_verts[n1])
			if(e1.isSource()) {
				auto n2 = m_edges[e1].to;
				if(bitmap[n2])
					return true;
				bitmap[n2] = true;
			}
		for(auto e1 : m_verts[n1])
			if(e1.isSource())
				bitmap[m_edges[e1].to] = false;
	}
	return false;
}

// TODO: zła nazwa
// Jak to ma działać z labelkami i trójkątami?
Graph Graph::makeForest(CSpan<Maybe<VertexId>> parents) {
	vector<pair<VertexId, VertexId>> edges;
	edges.reserve(parents.size());
	for(auto node_id : indexRange<VertexId>(parents))
		if(parents[node_id])
			edges.emplace_back(*parents[node_id], node_id);
	FATAL("writeme");
	return {};
}

bool Graph::isUndirected(Layers layers) const {
	for(auto idx : m_edges.indices())
		if(layers == all<Layer> || (layers & m_edge_layers[idx]))
			if(!findEdge(m_edges[idx].from, m_edges[idx].to, layers))
				return false;
	return true;
}

template <class T, EnableIf<is_scalar<T>>...>
Graph Graph::minimumSpanningTree(CSpan<T> edge_weights, bool as_undirected) const {
	DASSERT(edge_weights.size() == numEdges());

	if(numVerts() == 0)
		return {};

	vector<EdgeId> out;
	Heap<T> heap(numVerts());

	vector<bool> processed(numVerts(), false);
	vector<Maybe<VertexId>> pi(numVerts());
	vector<T> keys(pi.size(), inf);

	FATAL("fix me");
	m_verts.firstIndex();
	//keys[vertexIds()[0]] = T(0);

	for(auto node : vertexIds())
		heap.insert(node, keys[node]);

	while(!heap.empty()) {
		VertexId nid(heap.extractMin().second);
		processed[nid] = true;

		for(auto eid : m_verts[nid]) {
			if(!as_undirected && !eid.isSource())
				continue;

			auto nnode = other(eid, nid);
			auto weight = edge_weights[eid];

			if(!processed[nnode] && weight < keys[nnode]) {
				pi[nnode] = nid;
				keys[nnode] = weight;
				heap.update(nnode, weight);
			}
		}
	}

	return makeForest(pi);
}

Graph Graph::shortestPathTree(CSpan<VertexId> sources, CSpan<double> weights) const {
	Heap<double> heap(numVerts());
	vector<double> keys(numVerts(), inf);

	if(!weights.empty()) {
		DASSERT_EQ(weights.size(), numEdges());
		for(auto weight : weights)
			DASSERT_GE(weight, 0.0);
	}

	for(auto src_id : sources)
		keys[src_id] = 0.0;
	for(auto node_id : vertexIds())
		heap.insert(node_id, keys[node_id]);
	vector<bool> visited(numVerts(), false);

	vector<Maybe<VertexId>> out(numVerts());

	while(!heap.empty()) {
		VertexId nid(heap.extractMin().second);
		visited[nid] = true;

		for(auto eid : m_verts[nid])
			if(eid.isSource()) {
				auto target = m_edges[eid].to;
				if(visited[target])
					continue;

				auto new_key = (weights.empty() ? 1.0 : weights[eid]) + keys[nid];
				if(new_key < keys[target]) {
					out[target] = nid;
					keys[target] = new_key;
					heap.update(target, new_key);
				}
			}
	}

	return makeForest(out);
}

vector<Pair<VertexId>> Graph::edgePairs() const {
	vector<Pair<VertexId>> out;
	out.reserve(m_edges.size());
	for(auto [n1, n2] : m_edges)
		out.emplace_back(n1, n2);
	return out;
}

Graph Graph::reversed() const {
	vector<Pair<VertexId>> out;
	out.reserve(m_edges.size());
	for(auto [n1, n2] : m_edges)
		out.emplace_back(n2, n1);
	// TODO: what about labels?
	FATAL("writeme");
	return {};
	//return {m_labels_info, out};
}

bool Graph::hasCycles() const {
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
			auto node_id = stack.back().first;
			auto mode = stack.back().second;

			if(mode == enter) {
				if(status[node_id] != not_visited) {
					stack.pop_back();
					continue;
				}

				status[node_id] = visiting;

				stack.back().second = exit;
				for(auto eid : m_verts[node_id])
					if(eid.isSource()) {
						auto next_id = m_edges[eid].to;
						if(status[next_id] == visiting)
							return true;
						if(status[next_id] == not_visited)
							stack.emplace_back(next_id, enter);
					}
			} else {
				status[node_id] = visited;
				stack.pop_back();
			}
		}
	}

	return false;
}

bool Graph::isForest() const {
	if(hasCycles()) {
		print("cycle!\n");
		return false;
	}

	for(auto nid : vertexIds()) {
		int count = 0;
		for(auto eid : m_verts[nid])
			if(!eid.isSource()) {
				if(count > 1)
					return false;
				count++;
			}
	}
	return true;
}

vector<VertexId> Graph::treeRoots() const {
	vector<VertexId> out;
	for(auto nid : vertexIds()) {
		bool is_root = true;
		for(auto eid : m_verts[nid])
			if(!eid.isSource()) {
				is_root = false;
				break;
			}
		if(is_root)
			out.emplace_back(nid);
	}
	return out;
}

// TODO: is this really needed ?
int Graph::compare(const Graph &rhs) const {
	if(int cmp = m_verts.compare(rhs.m_verts))
		return cmp;
	if(int cmp = m_edges.compare(rhs.m_edges))
		return cmp;
	if(int cmp = m_tris.compare(rhs.m_tris))
		return cmp;

	FATAL("writeme");
	return 0;
}

// TODO: labels
FWK_ORDER_BY_DEF(Graph, m_verts, m_edges, m_tris)

template Graph Graph::minimumSpanningTree(CSpan<float>, bool) const;
template Graph Graph::minimumSpanningTree(CSpan<double>, bool) const;
}
