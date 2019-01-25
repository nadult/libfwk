// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/matrix3.h"

namespace fwk {

// Stored just like in OpenGL:
// Column major order, vector post multiplication
class Matrix4 {
  public:
	Matrix4() = default;
	Matrix4(const Matrix4 &) = default;
	Matrix4(const Matrix3 &mat) {
		v[0] = float4(mat[0], 0.0f);
		v[1] = float4(mat[1], 0.0f);
		v[2] = float4(mat[2], 0.0f);
		v[3] = float4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	Matrix4(const float4 &col0, const float4 &col1, const float4 &col2, const float4 &col3) {
		v[0] = col0;
		v[1] = col1;
		v[2] = col2;
		v[3] = col3;
	}
	Matrix4(CSpan<float, 16> values) {
		for(int n = 0; n < 4; n++)
			v[n] = float4(CSpan<float, 4>(values.data() + n * 4, 4));
	}

	static Matrix4 identity();
	static Matrix4 zero();

	float4 row(int n) const { return float4(v[0][n], v[1][n], v[2][n], v[3][n]); }

	float operator()(int row, int col) const { return v[col][row]; }
	float &operator()(int row, int col) { return v[col][row]; }

	const float4 &operator[](int n) const { return v[n]; }
	float4 &operator[](int n) { return v[n]; }

	Matrix4 operator+(const Matrix4 &) const;
	Matrix4 operator-(const Matrix4 &) const;
	Matrix4 operator*(float)const;

	Span<float4, 4> values() { return v; }
	CSpan<float4, 4> values() const { return v; }

	bool invert(const Matrix4 &);

	FWK_ORDER_BY(Matrix4, v[0], v[1], v[2], v[3]);

  private:
	float4 v[4];
};

static_assert(sizeof(Matrix4) == sizeof(float4) * 4, "Wrong size of Matrix4 class");

Matrix4 operator*(const Matrix4 &, const Matrix4 &);
float4 operator*(const Matrix4 &, const float4 &);

float3 mulPoint(const Matrix4 &mat, const float3 &);
float3 mulPointAffine(const Matrix4 &mat, const float3 &);
float3 mulNormal(const Matrix4 &inverse_transpose, const float3 &);
float3 mulNormalAffine(const Matrix4 &affine_mat, const float3 &);

// Equivalent to creating the matrix with column{0,1,2,3} as rows
Matrix4 transpose(const float4 &col0, const float4 &col1, const float4 &col2, const float4 &col3);
Matrix4 transpose(const Matrix4 &);

Matrix4 inverseOrZero(const Matrix4 &);

Matrix4 translation(const float3 &);
Matrix4 lookAt(const float3 &eye, const float3 &target, const float3 &up);
Matrix4 perspective(float fov, float aspect_ratio, float z_near, float z_far);
Matrix4 ortho(float left, float right, float top, float bottom, float near, float far);

DEFINE_ENUM(Orient2D, y_up, y_down);

// Simple 2D view with point (0, 0) in corner:
// - bottom left if orientation == Orient2D::y_up
// - top left if orientation == Orient2D::y_down
Matrix4 projectionMatrix2D(const IRect &viewport, Orient2D orientation);
Matrix4 viewMatrix2D(const IRect &viewport, const float2 &view_pos);

inline Matrix4 scaling(float x, float y, float z) { return scaling(float3(x, y, z)); }
inline Matrix4 scaling(float s) { return scaling(s, s, s); }
inline Matrix4 translation(float x, float y, float z) { return translation(float3(x, y, z)); }

// TODO: different name? transformPoints ?
Triangle3F operator*(const Matrix4 &, const Triangle3F &);
Plane3F operator*(const Matrix4 &, const Plane3F &);
Segment3F operator*(const Matrix4 &, const Segment3F &);
}
