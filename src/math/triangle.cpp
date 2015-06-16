/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

Triangle::Triangle(const float3 &a, const float3 &b, const float3 &c) {
	m_point = a;
	m_edge[0] = b - a;
	m_edge[1] = c - a;
	m_cross = fwk::cross(edge1(), edge2());
}

// TODO: write tests
float3 Triangle::normal() const { return normalize(m_cross); }
float Triangle::area() const { return 0.5f * length(m_cross); }

Triangle operator*(const Matrix4 &mat, const Triangle &tri) {
	return Triangle(mulPoint(mat, tri.a()), mulPoint(mat, tri.b()), mulPoint(mat, tri.c()));
}
}
