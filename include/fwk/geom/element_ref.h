// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"

namespace fwk {

template <class Ref, class Id> struct GRefs {
	PoolVector<Id> ids;
	const Graph *graph;

	struct Iter {
		constexpr const Iter &operator++() {
			ptr++;
			return *this;
		}
		Ref operator*() const { return Ref{graph, *ptr}; }
		constexpr int operator-(const Iter &rhs) const { return ptr - rhs.ptr; }
		constexpr bool operator==(const Iter &rhs) const { return ptr == rhs.ptr; }

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
	VertexRef(const Graph *graph, VertexId id) : m_graph(graph), m_id(id) {}

	operator VertexId() const { return m_id; }
	operator int() const { return (int)m_id; }
	VertexId id() const { return m_id; }
	int idx() const { return m_id.index(); }

	const GLabel *operator->() const;
	GLayers layers() const;

	EdgeRefs edges(GLayers = all<GLayer>) const;
	EdgeRefs edgesFrom(GLayers = all<GLayer>) const;
	EdgeRefs edgesTo(GLayers = all<GLayer>) const;

	EdgeRef next(EdgeId, bool source) const;
	EdgeRef prev(EdgeId, bool source) const;

	// Should we filter edges or vertices here ?
	VertexRefs vertsAdj(GLayers = all<GLayer>) const;
	VertexRefs vertsFrom(GLayers = all<GLayer>) const;
	VertexRefs vertsTo(GLayers = all<GLayer>) const;

	TriRefs tris(GLayers = all<GLayer>) const;

	int numEdges(GLayers = all<GLayer>) const;
	int numEdgesFrom(GLayers = all<GLayer>) const;
	int numEdgesTo(GLayers = all<GLayer>) const;

	template <auto v> explicit VertexRef(Intrusive::Tag<v> tag) : m_id(tag) {}
	template <auto v> bool operator==(Intrusive::Tag<v> tag) const { return m_id == tag; }

  private:
	friend class EdgeRef;
	friend class Graph;

	const Graph *m_graph;
	VertexId m_id;
};

class EdgeRef {
  public:
	EdgeRef(const Graph *graph, EdgeId id) : m_graph(graph), m_id(id) {}

	operator EdgeId() const { return m_id; }
	operator int() const { return (int)m_id; }
	EdgeId id() const { return m_id; }
	int idx() const { return m_id.index(); }

	const GLabel *operator->() const;

	Pair<VertexRef> verts() const;
	VertexRef from() const;
	VertexRef to() const;
	VertexRef other(VertexId node) const;
	GLayer layer() const;

	Maybe<EdgeRef> twin(GLayers = all<GLayer>) const;

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

	template <auto v> explicit EdgeRef(Intrusive::Tag<v> tag) : m_id(tag) {}
	template <auto v> bool operator==(Intrusive::Tag<v> tag) const { return m_id == tag; }

  private:
	friend class VertexRef;
	friend class Graph;

	const Graph *m_graph;
	EdgeId m_id;
};

class TriangleRef {
  public:
	TriangleRef(const Graph *graph, TriId id) : m_graph(graph), m_id(id) {}

	operator TriId() const { return m_id; }
	operator int() const { return (int)m_id; }
	TriId id() const { return m_id; }
	int idx() const { return m_id.index(); }

	const GLabel *operator->() const;

	array<VertexRef, 3> verts() const;
	VertexRef vert(int idx) const;
	GLayer layer() const;

	Maybe<EdgeRef> twin(GLayers = all<GLayer>) const;

	template <auto v> explicit TriangleRef(Intrusive::Tag<v> tag) : m_id(tag) {}
	template <auto v> bool operator==(Intrusive::Tag<v> tag) const { return m_id == tag; }

  private:
	friend class VertexRef;
	friend class Graph;

	const Graph *m_graph;
	TriId m_id;
};
}
