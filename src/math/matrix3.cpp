// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_math.h"

namespace fwk {

const Matrix3 Matrix3::identity() {
	return Matrix3({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
}

const Matrix3 transpose(const Matrix3 &mat) {
	return Matrix3({mat[0][0], mat[1][0], mat[2][0]}, {mat[0][1], mat[1][1], mat[2][1]},
				   {mat[0][2], mat[1][2], mat[2][2]});
}

const Matrix3 transpose(const float3 &a, const float3 &b, const float3 &c) {
	return Matrix3({a[0], b[0], c[0]}, {a[1], b[1], c[1]}, {a[2], b[2], c[2]});
}

const Matrix3 inverse(const Matrix3 &mat) {
	float3 out0, out1, out2;
	out0[0] = mat[1][1] * mat[2][2] - mat[1][2] * mat[2][1];
	out0[1] = mat[0][2] * mat[2][1] - mat[0][1] * mat[2][2];
	out0[2] = mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1];
	out1[0] = mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2];
	out1[1] = mat[0][0] * mat[2][2] - mat[0][2] * mat[2][0];
	out1[2] = mat[0][2] * mat[1][0] - mat[0][0] * mat[1][2];
	out2[0] = mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0];
	out2[1] = mat[0][1] * mat[2][0] - mat[0][0] * mat[2][1];
	out2[2] = mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];

	float det = mat[0][0] * out0[0] + mat[0][1] * out1[0] + mat[0][2] * out2[0];
	// TODO what if det close to 0?
	float invDet = 1.0f / det;

	return Matrix3(out0 * invDet, out1 * invDet, out2 * invDet);
}

const Matrix3 operator*(const Matrix3 &lhs, const Matrix3 &rhs) {
	Matrix3 tlhs = transpose(lhs);

	return Matrix3(float3(dot(rhs[0], tlhs[0]), dot(rhs[0], tlhs[1]), dot(rhs[0], tlhs[2])),
				   float3(dot(rhs[1], tlhs[0]), dot(rhs[1], tlhs[1]), dot(rhs[1], tlhs[2])),
				   float3(dot(rhs[2], tlhs[0]), dot(rhs[2], tlhs[1]), dot(rhs[2], tlhs[2])));
}

const float3 operator*(const Matrix3 &lhs, const float3 &rhs) {
	return float3(dot(lhs.row(0), rhs), dot(lhs.row(1), rhs), dot(lhs.row(2), rhs));
}

const Matrix3 rotation(const float3 &axis, float radians) {
	float cos = cosf(radians), sin = sinf(radians);
	float oneMinusCos = 1.0f - cos;

	float xx = axis[0] * axis[0];
	float yy = axis[1] * axis[1];
	float zz = axis[2] * axis[2];
	float xym = axis[0] * axis[1] * oneMinusCos;
	float xzm = axis[0] * axis[2] * oneMinusCos;
	float yzm = axis[1] * axis[2] * oneMinusCos;
	float xSin = axis[0] * sin;
	float ySin = axis[1] * sin;
	float zSin = axis[2] * sin;

	return transpose({xx * oneMinusCos + cos, xym - zSin, xzm + ySin},
					 {xym + zSin, yy * oneMinusCos + cos, yzm - xSin},
					 {xzm - ySin, yzm + xSin, zz * oneMinusCos + cos});
}

const Matrix3 scaling(const float3 &v) {
	return Matrix3({v[0], 0.0f, 0.0f}, {0.0f, v[1], 0.0f}, {0.0f, 0.0f, v[2]});
}
}
