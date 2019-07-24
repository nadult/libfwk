// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom/immutable_graph.h"
#include "fwk/geom/plane_graph.h"
#include "fwk/geom/plane_graph_builder.h"
#include "fwk/geom_base.h"
#include "fwk/vector_map.h"

namespace fwk {

// TODO: move it out of here ?
class Simplex {
  public:
	Simplex(VertexId node) : m_nodes{{node, no_init}}, m_size(1) {}
	Simplex(VertexId a, VertexId b) : m_nodes{{a, b}}, m_size(2) {}
	Simplex() : m_nodes{{no_init, no_init}}, m_size(0) {}

	bool isEdge() const { return m_size == 2; }
	bool isNode() const { return m_size == 1; }
	bool empty() const { return m_size == 0; }

	int size() const { return m_size; }
	VertexId operator[](int idx) const { return m_nodes[idx]; }

	VertexId asNode() const {
		DASSERT(isNode());
		return m_nodes[0];
	}
	Pair<VertexId> asEdge() const {
		DASSERT(isEdge());
		return {m_nodes[0], m_nodes[1]};
	}

	Simplex remap(const NodeMapping &mapping) const {
		if(isNode()) {
			if(auto node = mapping.maybeFind(m_nodes[0]))
				return *node;
		}
		if(isEdge()) {
			auto node1 = mapping.maybeFind(m_nodes[0]);
			auto node2 = mapping.maybeFind(m_nodes[1]);
			// TODO: check this
			return node1 && node2 ? Simplex(*node1, *node2) : Simplex();
		}
		return {};
	}

	const VertexId *begin() const { return &m_nodes[0]; }
	const VertexId *end() const { return begin() + m_size; }

	FWK_ORDER_BY(Simplex, m_size, m_nodes);

  private:
	// TODO: use staticvector
	array<VertexId, 2> m_nodes;
	int m_size = 0;
};

template <class Parent, class Base> auto makeParent(typename Parent::Base value) {
	return Parent();
}

struct VoronoiArcSegment {
	ArcId arc;
};

struct VoronoiArc {
	CellId cell;
	bool is_primary = false;
	bool touches_site = false;
};

struct VoronoiCell {
	VoronoiCell(Simplex generator, int type) : generator(generator), type(type) {}

	Simplex generator;
	int type;
};

struct VoronoiInfo {
	vector<double2> points;
	vector<VoronoiArc> arcs;
	vector<VoronoiArcSegment> segments;
	vector<VoronoiCell> cells;
};

// Arcs separate two neighbouring cells
// Arcs can be divided into segments, in most cases arc is composed of a single segment
class VoronoiDiagram {
  public:
	using Info = VoronoiInfo;
	using Arc = VoronoiArc;
	using ArcSegment = VoronoiArcSegment;
	using Cell = VoronoiCell;

	VoronoiDiagram() = default;
	VoronoiDiagram(ImmutableGraph arc_segments, ImmutableGraph arcs, Info);

	static vector<Pair<VertexId>> delaunay(CSpan<int2> sites);

	// After construction Cell generators will have same VertexId as in PGraph
	static VoronoiDiagram construct(const PGraph<int2> &);
	VoronoiDiagram clip(DRect) const;

	template <class Func> VoronoiDiagram transform(const Func &func) const {
		PGraph<double2> tgraph(m_segments_graph, m_info.points);
		auto transformed = tgraph.transform(func);
		if(transformed.numVerts() != m_segments_graph.numVerts())
			FWK_FATAL("Degenerate case"); // TODO: Expected<>
		Info new_info = m_info;
		new_info.points = transformed.points();
		return {transformed, m_arc_graph, move(new_info)};
	}

	// TODO: obliczenia i przechowywanie danych w różnych typach (float2 / rational2 / double2 ?)
	//       raczej nie teraz
	//
	// TODO: lepsza reprezentacja?
	// TODO: wierzchołek/segment początkowy nie łączy się z arcami
	// TODO: w trakcie dyskretyzacji tracę na dokładności; czy to jest problem?

	// TODO: node mapping
	// TODO: konwersje ArcId -> EdgeId i ArcSegmentId -> EdgeId...

	VectorMap<VertexId, VertexId> m_node_map; // Old node -> new node

	const auto &operator[](VertexId id) const {
		DASSERT(valid(id));
		return m_info.points[id];
	}
	const auto &operator[](ArcSegmentId id) const {
		DASSERT(valid(id));
		return m_info.segments[id];
	}
	const auto &operator[](ArcId id) const {
		DASSERT(valid(id));
		return m_info.arcs[id];
	}
	const auto &operator[](CellId id) const {
		DASSERT(valid(id));
		return m_info.cells[id];
	}

	Variant<None, double2, Segment2<double>> operator[](Simplex) const;

	const auto &points() const { return m_info.points; }
	const auto &arcSegments() const { return m_info.segments; }
	const auto &arcs() const { return m_info.arcs; }
	const auto &cells() const { return m_info.cells; }

	int numPoints() const { return m_info.points.size(); }
	int numSegments() const { return m_info.segments.size(); }
	int numArcs() const { return m_info.arcs.size(); }
	int numCells() const { return m_info.cells.size(); }

	bool valid(CellId id) const { return id >= 0 && id < m_info.cells.size(); }
	bool valid(ArcId id) const { return id >= 0 && id < m_info.arcs.size(); }
	bool valid(VertexId id) const { return id >= 0 && id < m_info.points.size(); }
	bool valid(ArcSegmentId id) const { return id >= 0 && id < m_info.segments.size(); }

	const auto &segmentGraph() const { return m_segments_graph; }
	const auto &arcGraph() const { return m_arc_graph; }
	const auto &info() const { return m_info; }

	bool empty() const { return m_info.points.empty(); }

	vector<vector<ArcSegmentId>> arcToSegments() const;
	vector<vector<ArcId>> cellToArcs() const;
	vector<CellId> segmentToCell() const;

	Maybe<CellId> findClosestCell(double2) const;

  private:
	ImmutableGraph m_arc_graph;
	ImmutableGraph m_segments_graph;
	Info m_info;
};
}
