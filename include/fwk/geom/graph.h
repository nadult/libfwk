#pragma once

#include "fwk/any.h"
#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/geom_base.h"
#include "fwk/gfx_base.h"
#include "fwk/hash_map.h"
#include "fwk/index_range.h"
#include "fwk/indexed_vector.h"
#include "fwk/small_vector.h"
#include "fwk/tag_id.h"
#include "fwk/type_info_gen.h"

namespace fwk {

// source means that current edge starts from current index
DEFINE_ENUM(EdgeOpt, source, flipped);
using EdgeFlags = EnumFlags<EdgeOpt>;

struct GraphLabel {
	u32 color = 0xffffffff;
	int ival1 = 0;
	int ival2 = 0;
	float fval1 = 0.0f;
	float fval2 = 0.0f;

	FWK_ORDER_BY_DECL(GraphLabel);
};

// TODO: Rename to EdgeId, after removing old class
struct GEdgeId {
	static constexpr int max_index = 1024 * 1024 * 16 - 2;
	static constexpr int uninitialized_index = max_index + 1;

	using Opt = EdgeOpt;

	GEdgeId(NoInitTag) : m_idx(uninitialized_index), m_flags(none) {}
	GEdgeId(int index, EdgeFlags flags = none) : m_idx(index), m_flags(flags) {
		PASSERT(index >= 0 && index < max_index);
	}
	GEdgeId(EdgeId id) : GEdgeId(id.index()) {}

	bool isSource() const { return m_flags & Opt::source; }
	bool isFlipped() const { return m_flags & Opt::flipped; }
	EdgeFlags flags() const { return m_flags; }

	explicit operator bool() const = delete;
	operator int() const { return PASSERT(isInitialized()), m_idx; }
	int index() const { return PASSERT(isInitialized()), m_idx; }

  private:
	bool isInitialized() const { return m_idx != uninitialized_index; }

	uint m_idx : 24;
	EdgeFlags m_flags;
};

class VertexRef {
  public:
	using EdgeId = GEdgeId;

	operator VertexId() const { return m_id; }
	operator int() const { return (int)m_id; }
	VertexId id() const { return m_id; }
	int idx() const { return m_id.index(); }

	const GraphLabel *operator->() const;

	auto edges() const;
	auto edgesFrom() const;
	auto edgesTo() const;

	auto nodesAdj() const;
	auto nodesFrom() const;
	auto nodesTo() const;

	int numEdges() const;
	int numEdgesFrom() const;
	int numEdgesTo() const;

	VertexRef(EmptyMaybe) : m_graph(nullptr), m_id(EmptyMaybe()) {}
	bool validMaybe() const { return m_graph; }

  private:
	VertexRef(const Graph *graph, VertexId id) : m_graph(graph), m_id(id) {}

	friend class EdgeRef;
	friend class Graph;

	const Graph *m_graph;
	VertexId m_id;
};

class EdgeRef {
  public:
	using EdgeId = GEdgeId;

	operator EdgeId() const { return m_id; }
	operator int() const { return (int)m_id; }
	EdgeId id() const { return m_id; }
	int idx() const { return m_id.index(); }

	const GraphLabel *operator->() const;

	VertexRef from() const;
	VertexRef to() const;
	VertexRef other(VertexId node) const;
	Maybe<EdgeRef> twin() const;

	bool adjacent(VertexId) const;
	bool adjacent(EdgeId) const;

	bool isSource() const { return m_id.isSource(); }
	bool isFlipped() const { return m_id.isFlipped(); } // TODO: handle it

	// -------------------------------------------------------------------------
	// -- Extended functions; extended info in graph must be present! ----------

	//EdgeRef nextFrom() const { return EdgeRef(*m_graph, m_graph->m_ext_info[m_id].next_from); }
	//EdgeRef prevFrom() const { return EdgeRef(*m_graph, m_graph->m_ext_info[m_id].prev_from); }
	//EdgeRef nextTo() const { return EdgeRef(*m_graph, m_graph->m_ext_info[m_id].next_to); }
	//EdgeRef prevTo() const { return EdgeRef(*m_graph, m_graph->m_ext_info[m_id].prev_to); }

	EdgeRef(EmptyMaybe) : m_graph(nullptr), m_id(EmptyMaybe()) {}
	bool validMaybe() const { return m_graph; }

  private:
	EdgeRef(const Graph *graph, EdgeId id) : m_graph(graph), m_id(id) {}
	friend class VertexRef;
	friend class Graph;

	const Graph *m_graph;
	EdgeId m_id;
};

// Simple Graph class with a focus on genericity & ease-of-use
// Besides verts & edges also have triangles support
// Each graph element can have an optional label
// TODO: add polygon support
class Graph {
  public:
	using EdgeId = GEdgeId;
	using Label = GraphLabel;

	Graph();
	FWK_COPYABLE_CLASS(Graph);

	bool empty() const { return m_verts.empty(); }
	int numNodes() const { return m_verts.size(); }
	int numEdges() const { return m_edges.size(); }
	int numTris() const { return m_tris.size(); }

	int endNodeIndex() const { return m_verts.endIndex(); }
	int endEdgeIndex() const { return m_edges.endIndex(); }
	int endTriIndex() const { return m_tris.endIndex(); }

	bool valid(EdgeId id) const { return m_edges.valid(id); }
	bool valid(VertexId id) const { return m_verts.valid(id); }
	bool valid(TriId id) const { return m_tris.valid(id); }

	// -------------------------------------------------------------------------------------------
	// ---  Access to graph elements -------------------------------------------------------------

	auto vertexIds() const { return m_verts.indices<VertexId>(); }
	auto edgeIds() const { return m_edges.indices<EdgeId>(); }

	Maybe<EdgeRef> find(VertexId, VertexId) const;

	CSpan<EdgeId> edges(VertexId id) const { return m_verts[id]; }
	int numEdges(VertexId node_id) const {
		return PASSERT(valid(node_id)), m_verts[node_id].size();
	}

	VertexRef ref(VertexId vertex_id) const {
		return PASSERT(valid(vertex_id)), VertexRef(this, vertex_id);
	}
	EdgeRef ref(EdgeId edge_id) const { return PASSERT(valid(edge_id)), EdgeRef(this, edge_id); }

	template <class TRange, class T>
	using EnableRangeOf = EnableIf<is_convertible<decltype(DECLVAL(TRange)[0]), T>>;

	auto refs(CSpan<VertexId> nodes) const {
		auto *graph = this;
		return IndexRange{0, nodes.size(), [=](int id) { return VertexRef(graph, VertexId(id)); }};
	}

	template <class Filter = None>
	auto refs(CSpan<EdgeId> edges, const Filter &filter = none) const {
		auto *graph = this;
		return IndexRange{0, edges.size(), [=](int id) { return EdgeRef(graph, EdgeId(id)); },
						  filter};
	}

	auto vertexRefs() const {
		auto *graph = this;
		auto *valids = m_verts.valids().data();
		return IndexRange{0, m_verts.endIndex(),
						  [=](int idx) { return VertexRef(graph, VertexId(idx)); },
						  [=](int idx) { return valids[idx]; }};
	}

	auto edgeRefs() const {
		auto *graph = this;
		auto *valids = m_edges.valids().data();
		return IndexRange{0, m_edges.endIndex(),
						  [=](int idx) { return EdgeRef(graph, EdgeId(idx)); },
						  [=](int idx) { return valids[idx]; }};
	}

	// TODO: naming: edgeIds ?
	auto edgesFrom(VertexId id) const {
		return refs(m_verts[id], [](EdgeId id) { return id.isSource(); });
	}
	auto edgesTo(VertexId id) const {
		return refs(m_verts[id], [](EdgeId id) { return !id.isSource(); });
	}

	vector<VertexId> nodesFrom(VertexId id) const;
	vector<VertexId> nodesTo(VertexId id) const;
	vector<VertexId> nodesAdj(VertexId id) const;

	VertexId other(EdgeId eid, VertexId nid) const {
		auto &edge = m_edges[eid];
		return edge.first == nid ? edge.second : edge.first;
	}

	VertexId from(EdgeId) const;
	VertexId to(EdgeId) const;

	const Label &operator[](VertexId) const;
	const Label &operator[](EdgeId) const;
	const Label &operator[](TriId) const;

	Label &operator[](VertexId);
	Label &operator[](EdgeId);
	Label &operator[](TriId);

	// -------------------------------------------------------------------------------------------
	// ---  Adding & removing elements -----------------------------------------------------------

	VertexId addVertex();
	template <class T> VertexId addVertex(PodVector<T> &points);

	pair<EdgeId, bool> add(VertexId, VertexId);
	EdgeId addNew(VertexId, VertexId);

	// TODO: a co jeśli mamy duplikat;
	// Co jeśli mamy trójkąt zwrócony w druga stronę?
	pair<TriId, bool> add(VertexId, VertexId, VertexId);
	TriId addNew(VertexId, VertexId, VertexId);

	void remove(VertexId);
	void remove(EdgeId);
	void remove(TriId);

	// Won't keep edge order
	void removeFast(EdgeId);

	void clear();

	void reserveVerts(int);
	void reserveEdges(int);
	void reserveTris(int);

	// -------------------------------------------------------------------------------------------
	// ---  Algorithms ---------------------------------------------------------------------------

	// TODO: functions which transform graph may drop some nodes with degree == 0

	// Missing twin edges will be added; TODO: better name: addMissingTwinEdges ?
	Graph asUndirected() const;
	// Evry edge has a twin; TODO: better name?
	bool isUndirected() const;

	// Edges are directed from parents to their children
	static Graph makeForest(CSpan<Maybe<VertexId>> parents);

	template <class T, EnableIf<is_scalar<T>>...>
	Graph minimumSpanningTree(CSpan<T> edge_weights, bool as_undirected = false) const;

	Graph shortestPathTree(CSpan<VertexId> sources,
						   CSpan<double> edge_weights = CSpan<double>()) const;

	vector<Pair<VertexId>> edgePairs() const;
	Graph reversed() const;

	bool hasEdgeDuplicates() const;

	bool hasCycles() const;
	bool isForest() const;
	vector<VertexId> treeRoots() const;

	int compare(const Graph &) const;
	FWK_ORDER_BY_DECL(Graph);

  protected:
	template <class> friend class GeomGraph;
	using NodeInfo = SmallVector<EdgeId, 7>;
	using EdgeInfo = Pair<VertexId>;
	using NodeTriInfo = SmallVector<TriId, 7>;

	struct ExtEdgeInfo {
		EdgeId next_from = no_init, prev_from = no_init;
		EdgeId next_to = no_init, prev_to = no_init;
	};

	// Czy trójkąt mapuje się na wierzchołki czy na krawędź?
	// To od algorytmu jaki wykonujemy na grafie zależy jakie dane będą nam potrzebne
	// - czasami lepsze są mapowania na wierzchołki, czasami indeksy krawędzi
	// - a może to i to ?
	struct TriangleInfo {
		array<VertexId, 3> verts;
		FWK_ORDER_BY(TriangleInfo, verts);
	};

	// Co z kolejnością krawędzi? Żeby ja ustanowić musze mieć jakąś płaszczyznę
	// OK, ale mogę mieć strukturę, która trzyma wszystkie podstawowe informacje, która
	// jest wspólna dla wszystkich innych grafów
	struct PolygonInfo {
		SmallVector<VertexId, 7> verts;
		FWK_ORDER_BY(PolygonInfo, verts);
	};

	template <class T> friend void orderEdges(Graph &, CSpan<T>);

	IndexedVector<NodeInfo> m_verts;
	IndexedVector<EdgeInfo> m_edges;
	IndexedVector<TriangleInfo> m_tris;
	vector<NodeTriInfo> m_vert_tris;

	HashMap<int, GraphLabel> m_vert_labels;
	HashMap<int, GraphLabel> m_edge_labels;
	HashMap<int, GraphLabel> m_tri_labels;

	friend class VertexRef;
	friend class EdgeRef;

	// TODO: optional m_edge_twin_info?
	//vector<ExtEdgeInfo> m_ext_info;
};

// -------------------------------------------------------------------------------------------
// ---  VertexRef & EdgeRef inline implementation --------------------------------------------

inline auto VertexRef::edges() const { return m_graph->refs(m_graph->m_verts[m_id]); }
inline auto VertexRef::edgesFrom() const {
	return m_graph->refs(m_graph->m_verts[m_id], [](EdgeId id) { return id.isSource(); });
}
inline auto VertexRef::edgesTo() const {
	return m_graph->refs(m_graph->m_verts[m_id], [](EdgeId id) { return !id.isSource(); });
}

// TODO: check if these are correct
inline auto VertexRef::nodesAdj() const { return m_graph->refs(m_graph->nodesAdj(m_id)); }
inline auto VertexRef::nodesFrom() const { return m_graph->refs(m_graph->nodesFrom(m_id)); }
inline auto VertexRef::nodesTo() const { return m_graph->refs(m_graph->nodesTo(m_id)); }

}
