// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"
#include "fwk/index_range.h"
#include "fwk/vector.h"

namespace fwk {

// TODO: new name ?
// TODO: drop it and just leave dynamic version?
class ImmutableGraph {
  public:
	class VertexRef;
	class EdgeRef;

	ImmutableGraph() = default;
	ImmutableGraph(CSpan<Pair<VertexId>>, Maybe<int> num_verts = none);

	bool hasExtendedInfo() const { return !!m_ext_info; }
	void computeExtendedInfo();

	// Edges are directed from parents to their children
	static ImmutableGraph makeForest(CSpan<Maybe<VertexId>> parents, Maybe<int> num_verts = none);

	Maybe<EdgeRef> findEdge(VertexId, VertexId) const;

	bool valid(EdgeId edge_id) const { return edge_id >= 0 && edge_id < m_edge_info.size(); }
	bool valid(VertexId vert_id) const { return vert_id >= 0 && vert_id < m_vert_info.size(); }

	VertexRef ref(VertexId vert_id) const {
		DASSERT(valid(vert_id));
		return VertexRef(*this, vert_id);
	}
	EdgeRef ref(EdgeId edge_id) const {
		DASSERT(valid(edge_id));
		return EdgeRef(*this, edge_id);
	}

	template <class TRange, class T>
	using EnableRangeOf = EnableIf<is_convertible<decltype(DECLVAL(TRange)[0]), T>>;

	template <class Range, EnableRangeOf<Range, VertexId>...> auto refs(Range vert_ids) const {
		return indexRange(move(vert_ids), [=](auto id) { return VertexRef(*this, id); });
	}

	template <class Range, EnableRangeOf<Range, EdgeId>...> auto refs(Range edge_ids) const {
		return indexRange(move(edge_ids), [=](auto id) { return EdgeRef(*this, id); });
	}

	class VertexRef {
	  public:
		operator VertexId() const { return m_id; }
		operator int() const { return (int)m_id; }
		VertexId id() const { return m_id; }
		int idx() const { return m_id.index(); }

		// TODO: edgesAdj() ?
		auto edges() const { return graph->refs(graph->edges(m_id)); }
		auto edgesFrom() const { return graph->refs(graph->edgesFrom(m_id)); }
		auto edgesTo() const { return graph->refs(graph->edgesTo(m_id)); }

		auto vertsAdj() const { return graph->refs(graph->vertsAdj(m_id)); }
		auto vertsFrom() const { return graph->refs(graph->vertsFrom(m_id)); }
		auto vertsTo() const { return graph->refs(graph->vertsTo(m_id)); }

		int numEdges() const {
			const auto &info = graph->m_vert_info[m_id];
			return info.num_edges_from + info.num_edges_to;
		}
		int numEdgesFrom() const { return graph->m_vert_info[m_id].num_edges_from; }
		int numEdgesTo() const { return graph->m_vert_info[m_id].num_edges_to; }

	  private:
		VertexRef(const ImmutableGraph &graph, VertexId id) : graph(&graph), m_id(id) {}
		friend class EdgeRef;
		friend class ImmutableGraph;

		const ImmutableGraph *graph;
		VertexId m_id;
	};

	class EdgeRef {
	  public:
		operator EdgeId() const { return m_id; }
		operator int() const { return (int)m_id; }
		EdgeId id() const { return m_id; }
		int idx() const { return m_id.index(); }

		VertexRef from() const { return VertexRef(*graph, graph->m_edge_info[m_id].first); }
		VertexRef to() const { return VertexRef(*graph, graph->m_edge_info[m_id].second); }
		VertexRef other(VertexId node) const {
			auto &edge = graph->m_edge_info[m_id];
			return VertexRef(*graph, edge.first == node ? edge.second : edge.first);
		}
		Maybe<EdgeRef> twin() const { return graph->findEdge(to(), from()); }

		bool adjacent(VertexId vert_id) const { return isOneOf(vert_id, from(), to()); }
		bool adjacent(EdgeId rhs_id) const {
			EdgeRef rhs(*graph, rhs_id);
			return adjacent(rhs.from()) || adjacent(rhs.to());
		}

		// -------------------------------------------------------------------------
		// -- Extended functions; extended info in graph must be present! ----------

		EdgeRef nextFrom() const { return EdgeRef(*graph, graph->m_ext_info[m_id].next_from); }
		EdgeRef prevFrom() const { return EdgeRef(*graph, graph->m_ext_info[m_id].prev_from); }
		EdgeRef nextTo() const { return EdgeRef(*graph, graph->m_ext_info[m_id].next_to); }
		EdgeRef prevTo() const { return EdgeRef(*graph, graph->m_ext_info[m_id].prev_to); }

	  private:
		EdgeRef(const ImmutableGraph &graph, EdgeId id) : graph(&graph), m_id(id) {}
		friend class VertexRef;
		friend class ImmutableGraph;

		const ImmutableGraph *graph;
		EdgeId m_id;
	};

	auto vertexIds() const { return indexRange<VertexId>(0, m_vert_info.size()); }
	auto edgeIds() const { return indexRange<EdgeId>(0, m_edge_info.size()); }

	auto vertexRefs() const {
		return indexRange(0, m_vert_info.size(),
						  [&](auto id) { return VertexRef(*this, VertexId(id)); });
	}
	auto edgeRefs() const {
		return indexRange(0, m_edge_info.size(),
						  [&](auto id) { return EdgeRef(*this, EdgeId(id)); });
	}

	bool empty() const { return m_vert_info.empty(); }

	// TODO: naming: edgeIds ?
	CSpan<EdgeId> edgesFrom(VertexId) const;
	CSpan<EdgeId> edgesTo(VertexId) const;
	CSpan<EdgeId> edges(VertexId) const;

	vector<VertexId> vertsFrom(VertexId) const;
	vector<VertexId> vertsTo(VertexId) const;
	vector<VertexId> vertsAdj(VertexId) const;

	VertexId from(EdgeId) const;
	VertexId to(EdgeId) const;

	int numEdges(VertexId vert_id) const {
		DASSERT(valid(vert_id));
		return m_vert_info[vert_id].num_edges_from + m_vert_info[vert_id].num_edges_to;
	}

	int numVerts() const { return m_vert_info.size(); }
	int numEdges() const { return m_edge_info.size(); }

	CSpan<pair<VertexId, VertexId>> edgePairs() const { return m_edge_info; }
	vector<array<VertexId, 3>> triangles() const;

	// Missing twin edges will be added; TODO: better name: addMissingTwinEdges ?
	ImmutableGraph asUndirected() const;
	bool isUndirected() const;

	template <class T, EnableIf<is_scalar<T>>...>
	ImmutableGraph minimumSpanningTree(CSpan<T> edge_weights, bool as_undirected = false) const;

	ImmutableGraph shortestPathTree(CSpan<VertexId> sources,
									CSpan<double> edge_weights = CSpan<double>()) const;

	ImmutableGraph reversed() const;

	bool hasEdgeDuplicates() const;

	bool hasCycles() const;
	bool isForest() const;
	vector<VertexId> treeRoots() const;

	FWK_ORDER_BY_DECL(ImmutableGraph, m_vert_info, m_edge_info, m_incidence_info);

  protected:
	struct VertexInfo {
		int num_edges_from = 0, num_edges_to = 0;
		int first_edge = 0;

		FWK_ORDER_BY_DECL(VertexInfo, num_edges_from, num_edges_to, first_edge);
	};

	using EdgeInfo = pair<VertexId, VertexId>;
	struct ExtEdgeInfo {
		EdgeId next_from = no_init, prev_from = no_init;
		EdgeId next_to = no_init, prev_to = no_init;
	};

	template <class T> friend void orderEdges(ImmutableGraph &, CSpan<T>);

	// TODO: optional m_edge_twin_info
	vector<VertexInfo> m_vert_info;
	vector<EdgeInfo> m_edge_info;
	vector<ExtEdgeInfo> m_ext_info;
	vector<EdgeId> m_incidence_info;
};

vector<pair<VertexId, VertexId>> remapVerts(CSpan<pair<VertexId, VertexId>>, CSpan<VertexId> map);
ImmutableGraph remapVerts(const ImmutableGraph &, CSpan<VertexId> map);
}
