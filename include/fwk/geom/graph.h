// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/any.h"
#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/geom/element_ref.h"
#include "fwk/geom_base.h"
#include "fwk/gfx_base.h"
#include "fwk/hash_map.h"
#include "fwk/index_range.h"
#include "fwk/small_vector.h"
#include "fwk/sparse_span.h"
#include "fwk/sparse_vector.h"

namespace fwk {

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
//
// TODO: operator[] mógłby działać jak ref;
// Potrzebujemy 3 różnych dostępów:
// - tworzenie refa z idka
// - zwracanie punktu / krawędzi trójkąta
// - dostęp do labelki
class Graph {
  public:
	using Label = GLabel;
	using Layer = GLayer;
	using Layers = GLayers;

	static constexpr int max_index = (1 << 27) - 1; // About ~134M

	struct VertexEdgeId {
		VertexEdgeId(int index, Layer layer, bool is_source)
			: value(uint(index) | (uint(layer) << 27) | (uint(is_source) << 30)) {}
		VertexEdgeId(NoInitTag) {}

		operator int() const { return value & max_index; }
		operator EdgeId() const { return EdgeId((int)(*this)); }
		Layer layer() const { return Layer((value >> 27) & 7); }
		bool isSource() const { return value & (1 << 30); }
		bool test(Layers mask) const { return mask == all<Layer> || (layer() & mask); }

		uint value;
	};

	struct VertexTriId {
		VertexTriId(int index, Layer layer, int vert_id)
			: value(uint(index) | (uint(layer) << 27) | (uint(vert_id) << 30)) {}
		VertexTriId(NoInitTag) {}

		operator int() const { return value & max_index; }
		operator TriId() const { return TriId((int)(*this)); }
		Layer layer() const { return Layer((value >> 27) & 7); }
		int vertId() const { return int(value >> 30); }
		bool test(Layers mask) const { return mask == all<Layer> || (layer() & mask); }

		uint value;
	};

	using VertexInfo = SmallVector<VertexEdgeId, 7>;
	using VertexTriInfo = SmallVector<VertexTriId, 7>;

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

	int vertsSpread() const { return m_verts.spread(); }
	int edgesSpread() const { return m_edges.spread(); }
	int trisSpread() const { return m_tris.spread(); }

	CSpan<bool> vertexValids() const { return m_verts.valids(); }
	CSpan<bool> edgeValids() const { return m_edges.valids(); }
	CSpan<bool> triValids() const { return m_tris.valids(); }

	SparseSpan<VertexInfo> vertexInfo() const;
	SparseSpan<Pair<VertexId>> edgePairs() const;

	// -------------------------------------------------------------------------------------------
	// ---  Access to graph elements -------------------------------------------------------------

	auto vertexIds() const { return m_verts.indices<VertexId>(); }
	auto edgeIds() const { return m_edges.indices<EdgeId>(); }
	auto triIds() const { return m_tris.indices<TriId>(); }

	vector<VertexId> vertexIds(Layers layer_mask) const;
	vector<EdgeId> edgeIds(Layers layer_mask) const;

	VertexRefs verts(Layers layers = all<Layer>) const { return {vertexIds(layers), this}; }
	EdgeRefs edges(Layers layers = all<Layer>) const { return {edgeIds(layers), this}; }

	Maybe<EdgeRef> findEdge(VertexId, VertexId, Layers = all<Layer>) const;
	Maybe<EdgeRef> findUndirectedEdge(VertexId, VertexId, Layers = all<Layer>) const;
	auto findEdges(VertexId, VertexId, Layers = all<Layer>) const;

	Maybe<TriId> findTri(VertexId, VertexId, VertexId, Layers = all<Layer>) const;
	auto findTris(VertexId, VertexId, VertexId, Layers = all<Layer>) const;

	VertexRef ref(VertexId vertex_id) const {
		return PASSERT(valid(vertex_id)), VertexRef(this, vertex_id);
	}
	EdgeRef ref(EdgeId edge_id) const { return PASSERT(valid(edge_id)), EdgeRef(this, edge_id); }

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
	bool isUndirected(Layers = all<Layer>) const;

	// Edges are directed from parents to their children
	static Graph makeForest(CSpan<Maybe<VertexId>> parents);

	template <class T, EnableIf<is_scalar<T>>...>
	Graph minimumSpanningTree(CSpan<T> edge_weights, bool as_undirected = false) const;

	Graph shortestPathTree(CSpan<VertexId> sources,
						   CSpan<double> edge_weights = CSpan<double>()) const;

	Graph reversed() const;

	bool hasEdgeDuplicates() const;

	bool hasCycles() const;
	bool isForest() const;
	vector<VertexId> treeRoots() const;
	vector<VertexId> topoSort(bool inverse, Layers = all<Layer>) const;

	int compare(const Graph &) const;
	FWK_ORDER_BY_DECL(Graph);

  protected:
	struct TopoSortCtx;
	void topoSort(TopoSortCtx &, VertexId) const;

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

	SparseVector<VertexInfo> m_verts; // TODO: better name
	PodVector<Layers> m_vert_layers;

	SparseVector<EdgeInfo> m_edges;
	PodVector<Layer> m_edge_layers;

	SparseVector<TriangleInfo> m_tris;
	PodVector<Layer> m_tri_layers;
	vector<VertexTriInfo> m_vert_tris;

	HashMap<int, Label> m_vert_labels;
	HashMap<int, Label> m_edge_labels;
	HashMap<int, Label> m_tri_labels;

	friend class VertexRef;
	friend class EdgeRef;

	// TODO: optional m_edge_twin_info?
	//vector<ExtEdgeInfo> m_ext_info;
};
}
