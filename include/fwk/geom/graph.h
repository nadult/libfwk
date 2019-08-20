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

DEFINE_ENUM(GraphLayer, l1, l2, l3, l4, l5, l6, l7, l8);
using GraphLayers = EnumFlags<GraphLayer>;

// Makes sense only for refs from verts
DEFINE_ENUM(TriVertexId, v1, v2, v3);

DEFINE_ENUM(GraphEdgeKind, from, to, both);

template <class Ref, class Id> struct GraphRefs {
	PoolVector<Id> ids;
	const Graph *graph;

	struct Iter {
		constexpr const Iter &operator++() {
			ptr++;
			return *this;
		}
		Ref operator*() const { return Ref{graph, *ptr}; }
		constexpr int operator-(const Iter &rhs) const { return ptr - rhs.ptr; }
		constexpr bool operator!=(const Iter &rhs) const { return ptr != rhs.ptr; }

		const Graph *graph;
		const Id *ptr;
	};

	Ref operator[](int idx) const { return Ref{graph, ids[idx]}; }
	Iter begin() const { return {graph, ids.begin()}; }
	Iter end() const { return {graph, ids.end()}; }
	int size() const { return ids.size(); }
};

using EdgeRefs = GraphRefs<EdgeRef, GEdgeId>;
using VertexRefs = GraphRefs<VertexRef, VertexId>;

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
	static constexpr int index_mask = 0x07ffffff; // ~130M
	static constexpr int max_index = index_mask - 1;

	using Opt = EdgeOpt;
	using Kind = GraphEdgeKind;

	GEdgeId(NoInitTag) { IF_PARANOID(m_value = ~0u); }
	explicit GEdgeId(int index, EdgeFlags flags = none, GraphLayer layer = GraphLayer::l1)
		: m_value(uint(index) | (uint(layer) << 27) | (flags.bits << 30)) {
		PASSERT(index >= 0 && index <= max_index);
	}

	bool isSource() const { return testInit(), m_value & (1 << (30 + (int)Opt::source)); }
	bool isFlipped() const { return testInit(), m_value & (1 << (30 + (int)Opt::flipped)); }
	EdgeFlags flags() const { return testInit(), EdgeFlags(m_value >> 30); }
	GraphLayer layer() const { return testInit(), GraphLayer((m_value >> 27) & 7); }

	bool test(GraphLayers layers) const {
		return layers == GraphLayers::all() || (layer() & layers);
	}

	explicit operator bool() const = delete;
	operator int() const { return testInit(), m_value & index_mask; }
	int index() const { return testInit(), m_value & index_mask; }

	GEdgeId(EmptyMaybe) : m_value(~0u) {}
	bool validMaybe() const { return m_value != ~0u; }

  private:
	void testInit() const { PASSERT(m_value != ~0u); }

	uint m_value;
};

struct GTriId {
	static constexpr int index_mask = 0x07ffffff; // ~130M
	static constexpr int max_index = index_mask - 1;

	GTriId(NoInitTag) { IF_PARANOID(m_value = ~0u); }
	explicit GTriId(int index, GraphLayer layer = GraphLayer::l1, TriVertexId vid = TriVertexId::v1)
		: m_value(uint(index) | (uint(layer) << 27) | (uint(vid) << 30)) {
		PASSERT(index >= 0 && index <= max_index);
	}
	GTriId(GTriId id, TriVertexId vid) : m_value((id.m_value & 0x3fffffff) | (uint(vid) << 30)) {}

	TriVertexId flags() const { return testInit(), TriVertexId(m_value >> 30); }
	GraphLayer layer() const { return testInit(), GraphLayer((m_value >> 27) & 7); }

	explicit operator bool() const = delete;
	operator int() const { return testInit(), m_value & index_mask; }
	int index() const { return testInit(), m_value & index_mask; }

	GTriId(EmptyMaybe) : m_value(~0u) {}
	bool validMaybe() const { return m_value != ~0u; }

  private:
	void testInit() const { PASSERT(m_value != ~0u); }

	uint m_value;
};

struct EdgeFilter {
	bool test_source;
	bool is_source;
	GraphLayers layers;

	bool operator()(GEdgeId id) const {
		return (!test_source || is_source == id.isSource()) &&
			   (layers == GraphLayers::all() || (id.layer() & layers));
	}
};

class VertexRef {
  public:
	using EdgeId = GEdgeId;
	using TriId = GTriId;
	using Layers = GraphLayers;

	VertexRef(const Graph *graph, VertexId id) : m_graph(graph), m_id(id) {}

	operator VertexId() const { return m_id; }
	operator int() const { return (int)m_id; }
	VertexId id() const { return m_id; }
	int idx() const { return m_id.index(); }

	const GraphLabel *operator->() const;

	EdgeRefs edges(Layers = Layers::all()) const;
	EdgeRefs edgesFrom(Layers = Layers::all()) const;
	EdgeRefs edgesTo(Layers = Layers::all()) const;

	VertexRefs vertsAdj() const;
	VertexRefs vertsFrom() const;
	VertexRefs vertsTo() const;

	int numEdges(Layers = Layers::all()) const;
	int numEdgesFrom(Layers = Layers::all()) const;
	int numEdgesTo(Layers = Layers::all()) const;

	VertexRef(EmptyMaybe) : m_graph(nullptr), m_id(EmptyMaybe()) {}
	bool validMaybe() const { return m_graph; }

  private:
	friend class EdgeRef;
	friend class Graph;

	const Graph *m_graph;
	VertexId m_id;
};

class EdgeRef {
  public:
	using EdgeId = GEdgeId;
	using TriId = GTriId;

	EdgeRef(const Graph *graph, EdgeId id) : m_graph(graph), m_id(id) {}

	operator EdgeId() const { return m_id; }
	operator int() const { return (int)m_id; }
	EdgeId id() const { return m_id; }
	int idx() const { return m_id.index(); }
	auto layer() const { return m_id.layer(); }

	const GraphLabel *operator->() const;

	VertexRef from() const;
	VertexRef to() const;
	VertexRef other(VertexId node) const;

	Maybe<EdgeRef> twin(GraphLayers = GraphLayers::all()) const;

	bool adjacent(VertexId) const;
	bool adjacent(EdgeId) const;

	bool isSource() const { return m_id.isSource(); }
	bool isFlipped() const { return m_id.isFlipped(); } // TODO: handle it

	// Using current order; TODO: enforce ordering somehow ?
	// TODO: layers ?
	EdgeRef prevFrom() const;
	EdgeRef nextFrom() const;
	EdgeRef prevTo() const;
	EdgeRef nextTo() const;

	// -------------------------------------------------------------------------
	// -- Extended functions; extended info in graph must be present! ----------

	//EdgeRef nextFrom() const { return EdgeRef(*m_graph, m_graph->m_ext_info[m_id].next_from); }
	//EdgeRef prevFrom() const { return EdgeRef(*m_graph, m_graph->m_ext_info[m_id].prev_from); }
	//EdgeRef nextTo() const { return EdgeRef(*m_graph, m_graph->m_ext_info[m_id].next_to); }
	//EdgeRef prevTo() const { return EdgeRef(*m_graph, m_graph->m_ext_info[m_id].prev_to); }

	EdgeRef(EmptyMaybe) : m_graph(nullptr), m_id(EmptyMaybe()) {}
	bool validMaybe() const { return m_graph; }

  private:
	friend class VertexRef;
	friend class Graph;

	const Graph *m_graph;
	EdgeId m_id;
};

// Simple Graph class with a focus on genericity & ease-of-use
// Besides verts & edges also have triangles support
// Each graph element can have an optional label
// Each element can lie on a different layer.
// Vertices can be assigned to multiple layers at once.
//
// There are two kinds of find functions:
// - one which returns first object which satisfies given condition
//   (returns Maybe<ObjectId>)
// - another one returns a range of all the objects which satisfy it
//
// There are 3 kinds of add functions:
// - [add*]: it simply adds new object with given parameter
// - [fix*]: only adds new object if another one with same parameters doesn't exist
// - [add*At]: adds new element at given index; Index should be free, otherwise it's an error
//   already; This function returns id and a bool which is true if new object was added
//
// TODO: add polygon support
class Graph {
  public:
	using EdgeId = GEdgeId;
	using TriId = GTriId;
	using Label = GraphLabel;
	using Layer = GraphLayer;
	using Layers = GraphLayers;
	using EdgeKind = GraphEdgeKind;

	using VertexInfo = SmallVector<EdgeId, 7>;
	using VertexTriInfo = SmallVector<TriId, 7>;

	Graph();
	Graph(CSpan<Pair<VertexId>>, Maybe<int> num_verts = none);
	FWK_COPYABLE_CLASS(Graph);

	template <class Id> struct FixedElem {
		Id id;
		bool is_new;
	};

	// TODO: informacja o layerach w VertexId
	// TODO: kopiowanie informacji o layerach do edge-idków i tri-idków dostępnych z poziomu grafu?

	bool empty() const { return m_verts.empty(); }
	int numVerts() const { return m_verts.size(); }
	int numEdges() const { return m_edges.size(); }
	int numTris() const { return m_tris.size(); }

	int numVerts(Layers) const;
	int numEdges(Layers) const;
	int numTris(Layers) const;

	bool valid(VertexId id) const { return m_verts.valid(id); }
	bool valid(EdgeId id) const { return m_edges.valid(id); }
	bool valid(TriId id) const { return m_tris.valid(id); }

	// -------------------------------------------------------------------------------------------
	// ---  Low level access ---------------------------------------------------------------------

	int vertsEndIndex() const { return m_verts.endIndex(); }
	int edgesEndIndex() const { return m_edges.endIndex(); }
	int trisEndIndex() const { return m_tris.endIndex(); }

	CSpan<bool> vertexValids() const { return m_verts.valids(); }
	CSpan<bool> edgeValids() const { return m_edges.valids(); }
	CSpan<bool> triValids() const { return m_tris.valids(); }

	CSpan<Pair<VertexId>> indexedEdges() const;
	CSpan<VertexInfo> indexedVerts() const;

	// -------------------------------------------------------------------------------------------
	// ---  Access to graph elements -------------------------------------------------------------

	auto vertexIds() const { return m_verts.indices<VertexId>(); }
	auto edgeIds() const { return m_edges.indices<EdgeId>(); }
	auto triIds() const { return m_tris.indices<TriId>(); }

	Maybe<EdgeRef> findEdge(VertexId, VertexId, Layers = Layers::all()) const;
	Maybe<EdgeRef> findUndirectedEdge(VertexId, VertexId, Layers = Layers::all()) const;
	auto findEdges(VertexId, VertexId, Layers = Layers::all()) const;

	Maybe<GTriId> findTri(VertexId, VertexId, VertexId, Layers = Layers::all()) const;
	auto findTris(VertexId, VertexId, VertexId, Layers = Layers::all()) const;

	CSpan<EdgeId> edges(VertexId id) const { return m_verts[id]; }
	int numEdges(VertexId node_id) const {
		return PASSERT(valid(node_id)), m_verts[node_id].size();
	}

	VertexRef ref(VertexId vertex_id) const {
		return PASSERT(valid(vertex_id)), VertexRef(this, vertex_id);
	}
	EdgeRef ref(EdgeId edge_id) const { return PASSERT(valid(edge_id)), EdgeRef(this, edge_id); }

	VertexRefs verts(Layers layer_mask = Layers::all()) const;
	EdgeRefs edges(Layers layer_mask = Layers::all()) const;

	VertexId other(EdgeId eid, VertexId nid) const {
		auto &edge = m_edges[eid];
		return edge.from == nid ? edge.to : edge.from;
	}

	// TODO: function to copy layer ?

	VertexId from(EdgeId) const;
	VertexId to(EdgeId) const;

	bool hasLabel(VertexId) const;
	bool hasLabel(EdgeId) const;

	// TODO: labelki powinny być na (), a punkty, segmenty na [] ? hmmmm...
	const Label &operator[](VertexId) const;
	const Label &operator[](EdgeId) const;
	const Label &operator[](TriId) const;

	Label &operator[](VertexId);
	Label &operator[](EdgeId);
	Label &operator[](TriId);

	Layers layers(VertexId) const;
	Layer layer(EdgeId) const;
	Layer layer(TriId) const;

	// -------------------------------------------------------------------------------------------
	// ---  Adding & removing elements -----------------------------------------------------------

	VertexId addVertex(Layers = Layer::l1);
	void addVertexAt(VertexId, Layers = Layer::l1);

	// Jak layery się do tego mają? można dodać drugą krawędź jeśli jest na innym layeru ?
	// może minimalizujmy ograniczenia; dodamy po prostu dwie funkcje do wszykiwania:
	// findFirst, find, która zwraca wektor albo SmallVector ? Można też po prostu zrobić filtr?
	//
	// Tylko na wierzchołki mamy ograniczenie w Geom Graphie ?
	EdgeId addEdge(VertexId, VertexId, Layer = Layer::l1);
	void addEdgeAt(EdgeId, VertexId, VertexId, Layer = Layer::l1);

	// Only adds new if identical edge doesn't exist already
	FixedElem<EdgeId> fixEdge(VertexId, VertexId, Layer = Layer::l1);
	FixedElem<EdgeId> fixUndirectedEdge(VertexId, VertexId, Layer = Layer::l1);

	TriId addTri(VertexId, VertexId, VertexId, Layer = Layer::l1);
	FixedElem<TriId> fixTri(VertexId, VertexId, VertexId, Layer = Layer::l1);

	void remove(VertexId);
	void remove(EdgeId);
	void remove(TriId);

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
	bool isUndirected(Layers = Layers::all()) const;

	// Edges are directed from parents to their children
	static Graph makeForest(CSpan<Maybe<VertexId>> parents);

	template <class T, EnableIf<is_scalar<T>>...>
	Graph minimumSpanningTree(CSpan<T> edge_weights, bool as_undirected = false) const;

	Graph shortestPathTree(CSpan<VertexId> sources,
						   CSpan<double> edge_weights = CSpan<double>()) const;

	// TODO: better name
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

	struct EdgeInfo {
		VertexId from, to;
		FWK_ORDER_BY(EdgeInfo, from, to);
	};

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

	IndexedVector<VertexInfo> m_verts; // TODO: better name
	PodVector<Layers> m_vert_layers;

	IndexedVector<EdgeInfo> m_edges;
	PodVector<Layer> m_edge_layers;

	IndexedVector<TriangleInfo> m_tris;
	PodVector<Layer> m_tri_layers;
	vector<VertexTriInfo> m_vert_tris;

	HashMap<int, GraphLabel> m_vert_labels;
	HashMap<int, GraphLabel> m_edge_labels;
	HashMap<int, GraphLabel> m_tri_labels;

	friend class VertexRef;
	friend class EdgeRef;

	// TODO: optional m_edge_twin_info?
	//vector<ExtEdgeInfo> m_ext_info;
};
}
