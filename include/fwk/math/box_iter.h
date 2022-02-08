// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/box.h"

namespace fwk {

template <class T> struct BoxCells {
	static_assert(is_vec<T, 2>);

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

		bool operator==(const Iter2D &rhs) const { return m_pos == rhs.m_pos; }
		bool operator<(const Iter2D &rhs) const {
			return m_pos[1] == rhs.m_pos[1] ? m_pos[0] < rhs.m_pos[0] : m_pos[1] < rhs.m_pos[1];
		}

	  private:
		T m_pos;
		Scalar m_begin_x, m_end_x;
	};

	Iter2D begin() const { return {m_min, m_min[0], m_max[0]}; }
	Iter2D end() const { return {T(m_min[0], m_max[1]), m_min[0], m_max[0]}; }

	T m_min, m_max;
};

// Iterate over unit-sized cells of given box
template <c_integral_vec<2> T> BoxCells<T> cells(const Box<T> &box) {
	return {box.min(), box.max()};
}
}
