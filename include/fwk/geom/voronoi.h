// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom/geom_graph.h"

namespace fwk {

using NodeMapping = VectorMap<VertexId, VertexId>;
using VoronoiCell = Variant<VertexId, EdgeId>;

// Arcs separate two neighbouring cells.
// Arcs can be divided into segments, in most cases arc is composed of a single segment.
// Sites are on layer 1, arcs on layer 2 and arc segments on layer 3
class Voronoi {
  public:
	using Cell = VoronoiCell;

	Voronoi() = default;
	Voronoi(GeomGraph<double2>, vector<Cell>);

	// applies to verts as well; Some verts are on multiple layers
	static constexpr auto site_layer = GLayer::l1;
	static constexpr auto arc_layer = GLayer::l2;
	static constexpr auto seg_layer = GLayer::l3;
	static constexpr auto clip_layer = GLayer::l4; // Only in verts

	static vector<Pair<VertexId>> delaunay(SparseSpan<int2>);

	// After construction Cell generators will have same VertexId as in PGraph
	static Ex<Voronoi> construct(const GeomGraph<int2> &);
	Ex<Voronoi> clip(DRect) const;

	bool isArcPrimary(EdgeId) const;
	EdgeId arcId(EdgeId) const;
	CellId cellId(EdgeId) const;

	// TODO: lepsza reprezentacja?
	// TODO: wierzchołek/segment początkowy nie łączy się z arcami
	// TODO: w trakcie dyskretyzacji tracę na dokładności; czy to jest problem?

	const auto &operator[](CellId id) const { return cells[id]; }
	bool valid(CellId id) const { return cells.inRange(id); }

	int numSegments() const { return graph.numEdges(seg_layer); }
	int numArcs() const { return graph.numEdges(seg_layer); }
	int numCells() const { return cells.size(); }

	vector<EdgeId> arcSegments(EdgeId) const;
	vector<EdgeId> cellArcs(CellId) const;

	Maybe<CellId> findClosestCell(double2) const;

	GeomGraph<double2> graph;
	vector<Cell> cells;
};
}
