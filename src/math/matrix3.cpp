// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/matrix3.h"

namespace fwk {

Matrix3 Matrix3::identity() {
	return Matrix3({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
}

Matrix3 transpose(const Matrix3 &mat) {
	return Matrix3({mat[0][0], mat[1][0], mat[2][0]}, {mat[0][1], mat[1][1], mat[2][1]},
				   {mat[0][2], mat[1][2], mat[2][2]});
}

Matrix3 transpose(const float3 &a, const float3 &b, const float3 &c) {
	return Matrix3({a[0], b[0], c[0]}, {a[1], b[1], c[1]}, {a[2], b[2], c[2]});
}

Matrix3 inverse(const Matrix3 &mat) {
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

Matrix3 Matrix3::operator*(const Matrix3 &rhs) const {
	Matrix3 tlhs = transpose(*this);

	return Matrix3(float3(dot(rhs[0], tlhs[0]), dot(rhs[0], tlhs[1]), dot(rhs[0], tlhs[2])),
				   float3(dot(rhs[1], tlhs[0]), dot(rhs[1], tlhs[1]), dot(rhs[1], tlhs[2])),
				   float3(dot(rhs[2], tlhs[0]), dot(rhs[2], tlhs[1]), dot(rhs[2], tlhs[2])));
}

float3 Matrix3::operator*(const float3 &rhs) const {
	return float3(dot(row(0), rhs), dot(row(1), rhs), dot(row(2), rhs));
}

Matrix3 rotation(const float3 &axis, float radians) {
	auto sc = sincos(radians);
	float oneMinusCos = 1.0f - sc.second;

	float xx = axis[0] * axis[0];
	float yy = axis[1] * axis[1];
	float zz = axis[2] * axis[2];
	float xym = axis[0] * axis[1] * oneMinusCos;
	float xzm = axis[0] * axis[2] * oneMinusCos;
	float yzm = axis[1] * axis[2] * oneMinusCos;
	float xSin = axis[0] * sc.first;
	float ySin = axis[1] * sc.first;
	float zSin = axis[2] * sc.first;

	return transpose({xx * oneMinusCos + sc.second, xym - zSin, xzm + ySin},
					 {xym + zSin, yy * oneMinusCos + sc.second, yzm - xSin},
					 {xzm - ySin, yzm + xSin, zz * oneMinusCos + sc.second});
}

}
