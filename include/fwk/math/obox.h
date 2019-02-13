// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"
#include "fwk/maybe.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vec, EnableInDimension<U, n>...>

// Oriented box (or rect in 2D case)
template <class T> class OBox {
  public:
	static_assert(is_vec<T>, "Box<> has to be based on a vec<>");
	static_assert(dim<T> == 2, "For now only 2D OBBs are supported");
	static constexpr int dim = fwk::dim<T>;
	static constexpr int num_corners = 1 << dim;

	using Scalar = typename T::Scalar;
	using Vec = T;
	using Vec2 = vec2<Scalar>;
	using Point = Vec;

	// TODO: check, that axes defined by corners are perpendicular
	OBox(CSpan<T, dim + 1> corners) : OBox(corners[0], corners[1], corners[2]) {}
	ENABLE_IF_SIZE(2) OBox(T c0, T c1, T c2) : m_corners{{c0, c1, c2}} {
		if(ccwSide(m_corners[1] - m_corners[0], m_corners[2] - m_corners[0]))
			swap(m_corners[1], m_corners[2]);
	}
	OBox() : m_corners{} {}

	OBox(const OBox &) = default;
	OBox &operator=(const OBox &) = default;

	template <class U, EnableIf<precise_conversion<U, T>>...>
	OBox(const OBox<U> &rhs) : OBox(T(rhs[0]), T(rhs[1]), T(rhs[2])) {}
	template <class U, EnableIf<!precise_conversion<U, T>>...>
	explicit OBox(const OBox<U> &rhs) : OBox(T(rhs[0]), T(rhs[1]), T(rhs[2])) {}

	bool isIntersecting(const OBox &) const;

	// Order: CW (in 2D case)
	array<T, num_corners> corners() const {
		return {m_corners[0], m_corners[1], m_corners[1] + m_corners[2] - m_corners[0],
				m_corners[2]};
	}

	const T &operator[](int idx) const {
		// Minimum number of corners is specified
		PASSERT(idx >= 0 && idx < dim + 1);
		return m_corners[idx];
	}

	array<T, dim + 1> m_corners;
	// TODO: Mogę zrobić generalizację wektora (którego mógłbym używac tutaj i np. w promieniu)
	// - na realach byłby znormalizowany
	// - na intach nie
};

#undef ENABLE_IF_SIZE
}
