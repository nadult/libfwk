// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom/geom_graph.h"

namespace fwk {

using NodeMapping = VectorMap<VertexId, VertexId>;

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

	Simplex remap(const NodeMapping &mapping) const;

	const VertexId *begin() const { return &m_nodes[0]; }
	const VertexId *end() const { return begin() + m_size; }

	FWK_ORDER_BY(Simplex, m_size, m_nodes);

  private:
	// TODO: use staticvector
	array<VertexId, 2> m_nodes;
	int m_size = 0;
};

// TODO: remove it ? put cells into graph
struct VoronoiCell {
	VoronoiCell(Simplex generator, int type) : generator(generator), type(type) {}

	Simplex generator;
	int type;
};

struct VoronoiInfo {
	vector<VoronoiCell> cells;
};

// Arcs separate two neighbouring cells.
// Arcs can be divided into segments, in most cases arc is composed of a single segment.
//
// Sites are on layer 1, arcs on layer 2 and arc segments on layer 3
// TODO: rename to Voronoi ?
// TODO: use EdgeIdx in some places?
class VoronoiDiagram {
  public:
	using Info = VoronoiInfo;
	using Cell = VoronoiCell; // TODO: remove it ?

	VoronoiDiagram() = default;
	VoronoiDiagram(GeomGraph<double2>, Info);

	// TODO: nicer interface?
	// arc segment labels:
	//   ival1: arc id
	//   ival2: cell id
	// arc:
	//   ival1: cell type, is_primary
	//   ival2: cell id

	// applies to verts as well; Some verts are on multiple layers
	static constexpr auto site_layer = GraphLayer::l1;
	static constexpr auto arc_layer = GraphLayer::l2;
	static constexpr auto seg_layer = GraphLayer::l3;

	static vector<Pair<VertexId>> delaunay(CSpan<int2> sites);

	// After construction Cell generators will have same VertexId as in PGraph
	static VoronoiDiagram construct(const GeomGraph<int2> &);
	VoronoiDiagram clip(DRect) const;

	bool isArcPrimary(EdgeId) const;
	EdgeId arcId(EdgeId) const;
	CellId cellId(EdgeId) const;

	/*
	template <class Func> VoronoiDiagram transform(const Func &func) const {
		PGraph<double2> tgraph(m_segments_graph, m_info.points);
		auto transformed = tgraph.transform(func);
		if(transformed.numVerts() != m_segments_graph.numVerts())
			FWK_FATAL("Degenerate case"); // TODO: Expected<>
		Info new_info = m_info;
		new_info.points = transformed.points();
		return {transformed, m_arc_graph, move(new_info)};
	}*/

	// TODO: obliczenia i przechowywanie danych w różnych typach (float2 / rational2 / double2 ?)
	//       raczej nie teraz
	//
	// TODO: lepsza reprezentacja?
	// TODO: wierzchołek/segment początkowy nie łączy się z arcami
	// TODO: w trakcie dyskretyzacji tracę na dokładności; czy to jest problem?

	const auto &operator[](CellId id) const {
		DASSERT(valid(id));
		return m_info.cells[id];
	}

	Variant<None, double2, Segment2<double>> operator[](Simplex) const;

	const auto &cells() const { return m_info.cells; }

	// TODO:
	int numSegments() const { return m_graph.numEdges(seg_layer); }
	int numArcs() const { return m_graph.numEdges(seg_layer); }
	int numCells() const { return m_info.cells.size(); }

	bool valid(CellId id) const { return id >= 0 && id < m_info.cells.size(); }

	const auto &graph() const { return m_graph; }
	const auto &info() const { return m_info; }

	bool empty() const { return m_graph.empty(); }

	vector<EdgeId> arcSegments(EdgeId) const;
	vector<EdgeId> cellArcs(CellId) const;

	Maybe<CellId> findClosestCell(double2) const;

  private:
	GeomGraph<double2> m_graph;
	Info m_info;
};
}
