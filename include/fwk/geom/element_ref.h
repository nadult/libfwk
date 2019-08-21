// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"

namespace fwk {

template <class Ref, class Id, bool pooled> struct GraphRefs {
	using Container = If<pooled, PoolVector<Id>, Vector<Id>>;
	Container ids;
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

class VertexRef {
  public:
	using Layers = GraphLayers;
	using Layer = GraphLayer;

	VertexRef(const Graph *graph, VertexId id) : m_graph(graph), m_id(id) {}

	operator VertexId() const { return m_id; }
	operator int() const { return (int)m_id; }
	VertexId id() const { return m_id; }
	int idx() const { return m_id.index(); }

	const GraphLabel *operator->() const;
	Layers layers() const;

	PooledEdgeRefs edges(Layers = all<Layer>) const;
	PooledEdgeRefs edgesFrom(Layers = all<Layer>) const;
	PooledEdgeRefs edgesTo(Layers = all<Layer>) const;

	// Should we filter edges or vertices here ?
	PooledVertexRefs vertsAdj(Layers = all<Layer>) const;
	PooledVertexRefs vertsFrom(Layers = all<Layer>) const;
	PooledVertexRefs vertsTo(Layers = all<Layer>) const;

	int numEdges(Layers = all<Layer>) const;
	int numEdgesFrom(Layers = all<Layer>) const;
	int numEdgesTo(Layers = all<Layer>) const;

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
	using Layer = GraphLayer;

	EdgeRef(const Graph *graph, EdgeId id) : m_graph(graph), m_id(id) {}

	operator EdgeId() const { return m_id; }
	operator int() const { return (int)m_id; }
	EdgeId id() const { return m_id; }
	int idx() const { return m_id.index(); }

	const GraphLabel *operator->() const;

	VertexRef from() const;
	VertexRef to() const;
	VertexRef other(VertexId node) const;
	GraphLayer layer() const;

	Maybe<EdgeRef> twin(GraphLayers = all<Layer>) const;

	bool adjacent(VertexId) const;
	bool adjacent(EdgeId) const;

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

}
