// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom/regular_grid.h"
#include "fwk/geom_base.h"
#include "fwk/math/segment.h"

namespace fwk {

// Iterates over cells lying on square border
struct SquareBorder {
	// radius has to be > 0
	SquareBorder(IRect clip_rect, int2 center, int radius);

	struct Iter {
		Iter(const SquareBorder &border, int dir);

		auto operator*() const { return pos; }
		const Iter &operator++();

		FWK_ORDER_BY(Iter, dir, steps);

	  private:
		const SquareBorder &border;
		int2 pos;
		short dir, steps;
	};

	Iter begin() const { return {*this, 0}; }
	Iter end() const { return {*this, 4}; }

  private:
	int2 start[4];
	short steps[5];
	friend struct Iter;
};

// Groups segments and points into cells; stores indices only
// It's designed for evenly distributed sets of segments where
// each of them span over small number of cells
template <class T> class SegmentGrid {
  public:
	static_assert(is_vec<T, 2>, "");
	using Vector = T;
	using Scalar = fwk::Scalar<T>;
	using Segment = fwk::Segment<T>;
	using RGrid = RegularGrid<T, int2>;
	using Rect = typename RGrid::Rect;
	using IRect = typename RGrid::IRect;
	using Point = Vector;

	using IsectParam = Pair<fwk::IsectParam<Scalar>, EdgeId>;
	using Isect = Variant<None, T, Segment>;

	// Opcje:
	// - generycznie: funkcja traverse która bierze On

	// - zwraca identyfikatory
	// - zwraca identyfikatory + parametry przecięć
	// - zwraca iterator?

	SegmentGrid(CSpan<Pair<VertexId>>, CSpan<Point>, CSpan<bool> valid_edges = {},
				CSpan<bool> valid_points = {});
	SegmentGrid() = default;

	// Zamiast tych funkcji ma jedynie funkcje dostępu do komórek i
	// ew. trace-owania po segmencie ?
	//
	// Grid jest w końcu używany jako element większych klas...

	struct Cell {
		bool empty() const { return num_verts == 0 && num_edges == 0; }

		// TODO: naming: points/nodes etc.
		int num_verts = 0, num_edges = 0;
		int first_index = 0;

		// TODO: precompute these and use in algos
		//char next_x = 0, next_y = 0;
		//char prev_x = 0, prev_y = 0;
	};

	const auto &grid() const { return m_grid; }
	int2 size() const { return m_grid.size(); }
	int width() const { return m_grid.width(); }
	int height() const { return m_grid.height(); }

	int index(int2 cell_id) const { return cell_id.x + cell_id.y * m_grid.width(); }
	bool inRange(int2 cell_id) const { return m_grid.inRange(cell_id); }

	int2 toCell(T world_id) const { return m_grid.toCell(world_id); }
	IRect toCell(const Rect &world_rect) const { return m_grid.toCellRect(world_rect); }

	// TODO: sometimes when converting points to cells, we're out of borders
	// We have to handle such situations
	//
	// Because of inaccuracies point may end up in different cell than expected;
	// How to deal with this robustly ?

	CSpan<VertexId> cellVerts(int2 cell_id) const {
		auto &cell = m_cells[index(cell_id)];
		auto *ptr = &m_cell_indices[cell.first_index];
		return span(ptr, cell.num_verts).template reinterpret<VertexId>();
	}
	CSpan<EdgeId> cellEdges(int2 cell_id) const {
		auto &cell = m_cells[index(cell_id)];
		auto *ptr = m_cell_indices.data() + cell.first_index + cell.num_verts;
		return span(ptr, cell.num_edges).template reinterpret<EdgeId>();
	}

	const Cell &operator[](int2 cell_id) const {
		DASSERT(inRange(cell_id));
		auto idx = index(cell_id);
		return m_cells[idx];
	}

	bool empty(int2 cell_id) const { return !inRange(cell_id) || m_cells[index(cell_id)].empty(); }

	// In case of integer grids, you have to clip tested segments, because they may
	// cause overflow errors otherwise
	// Allow operations on bigger ints than 32bit ?

	vector<int2> traceSlow(const Segment &) const;
	PoolVector<int2> trace(const Segment &) const;

	// This one if performed on doubles for int-based grids
	Maybe<EdgeId> closestEdge(const Point &, CSpan<Segment>, Scalar max_dist = inf) const;
	Maybe<VertexId> closestVertex(const Point &, CSpan<Point> ref_points, Scalar max_dist = inf,
								  Maybe<VertexId> ignore = none) const;

	auto begin() const { return m_grid.begin(); }
	auto end() const { return m_grid.end(); }

  private:
	static RGrid bestGrid(CSpan<T>, CSpan<bool>, int num_edges);

	T cellCorner(int2 cell_id) const { return m_grid.toWorld(cell_id); }

	// This is inaccurate
	Pair<int2> clipSegment(Segment) const;
	T clipSegmentPoint(T point, T vector) const;

	RGrid m_grid;
	vector<Cell> m_cells;
	vector<VertexId::Base> m_cell_indices;
};
}
