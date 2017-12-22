// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/box.h"

namespace fwk {

template <class T> struct BoxCells {
	static_assert(isIntegralVec<T, 2>());

	using Scalar = typename T::Scalar;

	class Iter2D {
	  public:
		Iter2D(T pos, Scalar begin_x, Scalar end_x)
			: m_pos(pos), m_begin_x(begin_x), m_end_x(end_x) {}
		auto operator*() const { return m_pos; }
		const Iter2D &operator++() {
			m_pos[0]++;
			if(m_pos[0] >= m_end_x) {
				m_pos[0] = m_begin_x;
				m_pos[1]++;
			}
			return *this;
		}

		FWK_ORDER_BY(Iter2D, m_pos.y, m_pos.x);

	  private:
		T m_pos;
		Scalar m_begin_x, m_end_x;
	};

	template <class U = T, EnableIf<isIntegralVec<U, 2>()>...> Iter2D begin() const {
		return {m_min, m_min[0], m_max[0]};
	}
	template <class U = T, EnableIf<isIntegralVec<U, 2>()>...> Iter2D end() const {
		return {T(m_min[0], m_max[1]), m_min[0], m_max[0]};
	}

	T m_min, m_max;
};

// Iterate over unit-sized cells of given box
template <class T, EnableIf<isIntegralVec<T, 2>()>...> BoxCells<T> cells(const Box<T> &box) {
	return {box.min(), box.max()};
}
}
