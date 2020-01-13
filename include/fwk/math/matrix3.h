// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"

namespace fwk {

// Stored just like in OpenGL:
// Column major order, vector post multiplication
class Matrix3 {
  public:
	Matrix3() = default;
	Matrix3(const Matrix3 &) = default;
	Matrix3(const float3 &col0, const float3 &col1, const float3 &col2) {
		v[0] = col0;
		v[1] = col1;
		v[2] = col2;
	}

	static Matrix3 identity();
	static Matrix3 scaling(const float3 &v) {
		return {{v[0], 0.0f, 0.0f}, {0.0f, v[1], 0.0f}, {0.0f, 0.0f, v[2]}};
	}

	float3 row(int n) const { return float3(v[0][n], v[1][n], v[2][n]); }

	float operator()(int row, int col) const { return v[col][row]; }
	float &operator()(int row, int col) { return v[col][row]; }

	// Access columns
	const float3 &operator[](int n) const { return v[n]; }
	float3 &operator[](int n) { return v[n]; }

	Matrix3 operator*(const Matrix3 &) const;
	float3 operator*(const float3 &) const;

	Span<float3, 3> values() { return v; }
	CSpan<float3, 3> values() const { return v; }

	FWK_ORDER_BY(Matrix3, v[0], v[1], v[2]);

  private:
	float3 v[3];
};

static_assert(sizeof(Matrix3) == sizeof(float3) * 3, "Wrong size of Matrix3 class");

Matrix3 transpose(const Matrix3 &);

// Equivalent to creating the matrix with column{0,1,2} as rows
Matrix3 transpose(const float3 &column0, const float3 &column1, const float3 &column2);
Matrix3 inverse(const Matrix3 &);
Matrix3 rotation(const float3 &axis, float angle);
}
