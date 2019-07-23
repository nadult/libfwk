// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/regular_grid.h"

#include "fwk/sys/assert.h"

namespace fwk {

template <class T, class IT>
RegularGrid<T, IT>::RegularGrid(Rect rect, T cell_size, IScalar border)
	: m_offset(rect.min()), m_cell_size(cell_size), m_border(border) {
	DASSERT_GT(cell_size.x, Scalar(0));
	DASSERT_GT(cell_size.y, Scalar(0));

	if constexpr(is_fpt) {
		m_inv_cell_size = vinv(m_cell_size);
		T almost_one(Scalar(1) - epsilon<Scalar>);
		m_size = IT(rect.size() * m_inv_cell_size + almost_one) + IT(border, border) * 2;
	} else {
		auto size = rect.size();
		m_size = IT(size[0] / m_cell_size[0], size[1] / m_cell_size[1]) + IT(border, border) * 2;
	}
	m_offset -= T(border) * m_cell_size;
}

template <class T, class IT>
RegularGrid<T, IT>::RegularGrid(T offset, IT size, T cell_size)
	: m_size(size), m_offset(offset), m_cell_size(cell_size) {
	DASSERT_GT(cell_size.x, Scalar(0));
	DASSERT_GT(cell_size.y, Scalar(0));
	DASSERT_GE(size.x, IScalar(0));
	DASSERT_GE(size.y, IScalar(0));

	if constexpr(is_fpt)
		m_inv_cell_size = vinv(m_cell_size);
}

template class RegularGrid<short2, int2>;
template class RegularGrid<int2, int2>;
template class RegularGrid<float2, int2>;
template class RegularGrid<double2, int2>;
}
