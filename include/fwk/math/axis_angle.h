// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/matrix3.h"
#include "fwk_math.h"

namespace fwk {

// TODO: make sure that all classes / structures here have proper default constructor (for
// example AxisAngle requires fixing)

class AxisAngle {
  public:
	AxisAngle() {}
	AxisAngle(const float3 &axis, float angle) : m_axis(normalize(axis)), m_angle(angle) {}
	inline operator const Matrix3() const { return rotation(m_axis, m_angle); }

	float angle() const { return m_angle; }
	const float3 axis() const { return m_axis; }

	FWK_ORDER_BY(AxisAngle, m_axis, m_angle);

  private:
	float3 m_axis;
	float m_angle;
};
}
