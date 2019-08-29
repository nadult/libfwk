// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"
#include "fwk/math/box_iter.h"

namespace fwk {

template <class T, EnableIfVec<T, 2>...> array<T, 9> nearby9Cells(T pos) {
	return array<T, 9>{{pos, pos + T(1, 0), pos + T(1, 1), pos + T(0, 1), pos + T(-1, 1),
						pos + T(-1, 0), pos + T(-1, -1), pos + T(0, -1), pos + T(1, -1)}};
}

// There are 3 spaces here (world, grid and cell)
// grid and cell are the same, but cell is rounded to ints
// TODO: explain it better
template <class T, class IT> class RegularGrid {
  public:
	static_assert(is_vec<T, 2>, "");
	static_assert(is_integral<Base<IT>> && is_vec<IT, 2>, "");

	using Vector = T;
	using Scalar = fwk::Scalar<T>;
	using IScalar = fwk::Scalar<IT>;
	using Rect = Box<T>;
	using IRect = Box<IT>;
	static constexpr bool is_fpt = fwk::is_fpt<Scalar>;

	RegularGrid(Rect rect, T cell_size, IScalar border = 0);
	RegularGrid(Rect rect, Scalar cell_size, IScalar border = 0)
		: RegularGrid(rect, T(cell_size), border) {}
	RegularGrid(T offset, IT size, T cell_size);
	RegularGrid() : RegularGrid(Rect(), T(1)) {}

	IT size() const { return m_size; }
	IScalar width() const { return m_size[0]; }
	IScalar height() const { return m_size[1]; }

	// TODO: world -> real
	T toWorld(T grid_pos) const { return grid_pos * m_cell_size + m_offset; }
	template <class U = T, EnableIf<!is_same<U, IT>>...> T toWorld(IT cell_pos) const {
		return toWorld(T(cell_pos));
	}

	//TODO: option to operate on powers-of two only? (shifts instead of mul & div)

	// TODO: use vceil in rect.max() ?
	Rect toWorld(Rect grid_rect) const {
		return {toWorld(grid_rect.min()), toWorld(grid_rect.max())};
	}
	template <class U = T, EnableIf<!is_same<U, IT>>...> Rect toWorld(IRect cell_rect) const {
		return {toWorld(cell_rect.min()), toWorld(cell_rect.max())};
	}

	Rect toWorldRect(IT cell_pos) const { return toWorld(IRect(cell_pos, cell_pos + IT(1))); }

	T toGrid(T world_pos) const {
		if(is_fpt)
			return (world_pos - m_offset) * m_inv_cell_size;
		else {
			auto rel = world_pos - m_offset;
			return {Scalar(rel[0] / m_cell_size[0]), Scalar(rel[1] / m_cell_size[1])};
		}
	}

	IT toCell(T world_pos) const {
		if constexpr(is_fpt)
			return IT(vfloor(toGrid(world_pos)));
		else
			return IT(toGrid(world_pos));
	}

	IRect toCellRect(Rect world_rect) const {
		auto cmin = toCell(world_rect.min());
		auto cmax = toCell(world_rect.max());
		return {cmin, cmax + IT(1)};
	}

	T worldRemainder(IT cell_pos, T world_pos) const {
		return (world_pos - m_offset) - T(cell_pos) * m_cell_size;
	}

	template <class U = T, EnableIfFptVec<U>...> static pair<IT, T> cellRemainder(T grid_pos) {
		IT cell_pos(vfloor(grid_pos));
		T remainder(grid_pos.x - cell_pos.x, grid_pos.y - cell_pos.y);
		return {cell_pos, remainder};
	}

	IRect cellRect() const { return IRect(m_size); }
	Rect worldRect() const { return Rect(m_offset, m_offset + T(m_size) * m_cell_size); }

	T cellSize() const { return m_cell_size; }
	T offset() const { return m_offset; }
	IScalar border() const { return m_border; }

	bool inRange(IT pos) const {
		return pos.x >= 0 && pos.y >= 0 && pos.x < m_size.x && pos.y < m_size.y;
	}

	array<IT, 4> neighbours4(IT pos) const {
		return array<IT, 4>{{pos + IT(1, 0), pos + IT(0, 1), pos + IT(-1, 0), pos + IT(0, -1)}};
	}
	array<IT, 8> neighbours8(IT pos) const {
		return array<IT, 8>{{pos + IT(1, 0), pos + IT(1, 1), pos + IT(0, 1), pos + IT(-1, 1),
							 pos + IT(-1, 0), pos + IT(-1, -1), pos + IT(0, -1), pos + IT(1, -1)}};
	}
	array<IT, 9> nearby9(IT pos) const { return nearby9Cells(pos); }

	auto begin() const { return cells(cellRect()).begin(); }
	auto end() const { return cells(cellRect()).end(); }

	auto tied() const { return std::tie(m_size, m_offset, m_cell_size, m_border); }
	bool operator==(const RegularGrid &rhs) const { return tied() == rhs.tied(); }

  private:
	IT m_size;
	T m_offset;
	T m_cell_size, m_inv_cell_size;
	IScalar m_border = 0;
};
}
