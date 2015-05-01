/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

const Matrix4 Matrix4::zero() {
	Matrix4 out;
	for(int n = 0; n < 4; n++)
		out[n] = float4(0, 0, 0, 0);
	return out;
}
	
const Matrix4 Matrix4::operator+(const Matrix4 &rhs) const {
	Matrix4 out;
	for(int n = 0; n < 4; n++)
		out[n] = v[n] + rhs[n];
	return out;
}

const Matrix4 Matrix4::operator-(const Matrix4 &rhs) const {
	Matrix4 out;
	for(int n = 0; n < 4; n++)
		out[n] = v[n] + rhs[n];
	return out;
}

const Matrix4 Matrix4::operator*(float s) const {
	Matrix4 out;
	for(int n = 0; n < 4; n++)
		out[n] = v[n] * s;
	return out;
}

const Matrix4 operator*(const Matrix4 &lhs, const Matrix4 &rhs) {
	Matrix4 tlhs = transpose(lhs);
	return Matrix4(
	   float4(dot(rhs[0], tlhs[0]), dot(rhs[0], tlhs[1]), dot(rhs[0], tlhs[2]), dot(rhs[0], tlhs[3])),
	   float4(dot(rhs[1], tlhs[0]), dot(rhs[1], tlhs[1]), dot(rhs[1], tlhs[2]), dot(rhs[1], tlhs[3])),
	   float4(dot(rhs[2], tlhs[0]), dot(rhs[2], tlhs[1]), dot(rhs[2], tlhs[2]), dot(rhs[2], tlhs[3])),
	   float4(dot(rhs[3], tlhs[0]), dot(rhs[3], tlhs[1]), dot(rhs[3], tlhs[2]), dot(rhs[3], tlhs[3])));
}

const float4 operator*(const Matrix4 &lhs, const float4 &rhs) {
	return float4(	
		dot(lhs.row(0), rhs),
		dot(lhs.row(1), rhs),
		dot(lhs.row(2), rhs),
		dot(lhs.row(3), rhs));
}

const float3 mulPoint(const Matrix4 &mat, const float3 &pt) {
	float4 tmp = mat * float4(pt, 1.0f);
	return tmp.xyz() / tmp.w;
}

const float3 mulPointAffine(const Matrix4 &mat, const float3 &pt) {
	return float3(	dot(mat.row(0).xyz(), pt),
					dot(mat.row(1).xyz(), pt),
					dot(mat.row(2).xyz(), pt)) + mat[3].xyz();
}
	
const float3 mulNormal(const Matrix4 &invTrans, const float3 &nrm) {
	return float3(	dot(invTrans.row(0).xyz(), nrm),
					dot(invTrans.row(1).xyz(), nrm),
					dot(invTrans.row(2).xyz(), nrm));
}

const float3 mulNormalAffine(const Matrix4 &invTrans, const float3 &nrm) {
	float4 tmp = invTrans * float4(nrm[0], nrm[1], nrm[2], 0.0f);
	return tmp.xyz();
}

bool operator==(const Matrix4 &lhs, const Matrix4 &rhs) {
	return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2] && lhs[3] == rhs[3];
}

bool operator!=(const Matrix4 &lhs, const Matrix4 &rhs) {
	return !(lhs == rhs);
}

const Matrix4 transpose(const float4 &a, const float4 &b, const float4 &c, const float4 &d) {
	return Matrix4(
			{ a[0], b[0], c[0], d[0] },
			{ a[1], b[1], c[1], d[1] },
			{ a[2], b[2], c[2], d[2] },
		    { a[3], b[3], c[3], d[3] } );
}

const Matrix4 inverse(const Matrix4 &mat) {
	Matrix4 out;

	float t[12], m[16];

	for(size_t n = 0; n < 4; n++) {
		m[n + 0]  = mat[n][0];
		m[n + 4]  = mat[n][1];
		m[n + 8]  = mat[n][2];
		m[n + 12] = mat[n][3];
	}

	t[0]  = m[10] * m[15];
	t[1]  = m[11] * m[14];
	t[2]  = m[9 ] * m[15];
	t[3]  = m[11] * m[13];
	t[4]  = m[9 ] * m[14];
	t[5]  = m[10] * m[13];
	t[6]  = m[8 ] * m[15];
	t[7]  = m[11] * m[12];
	t[8]  = m[8 ] * m[14];
	t[9]  = m[10] * m[12];
	t[10] = m[8 ] * m[13];
	t[11] = m[9 ] * m[12];

	out[0][0]  = t[0] * m[5] + t[3] * m[6] + t[4 ] * m[7];
	out[0][0] -= t[1] * m[5] + t[2] * m[6] + t[5 ] * m[7];
	out[0][1]  = t[1] * m[4] + t[6] * m[6] + t[9 ] * m[7];
	out[0][1] -= t[0] * m[4] + t[7] * m[6] + t[8 ] * m[7];
	out[0][2]  = t[2] * m[4] + t[7] * m[5] + t[10] * m[7];
	out[0][2] -= t[3] * m[4] + t[6] * m[5] + t[11] * m[7];
	out[0][3]  = t[5] * m[4] + t[8] * m[5] + t[11] * m[6];
	out[0][3] -= t[4] * m[4] + t[9] * m[5] + t[10] * m[6];
	out[1][0]  = t[1] * m[1] + t[2] * m[2] + t[5 ] * m[3];
	out[1][0] -= t[0] * m[1] + t[3] * m[2] + t[4 ] * m[3];
	out[1][1]  = t[0] * m[0] + t[7] * m[2] + t[8 ] * m[3];
	out[1][1] -= t[1] * m[0] + t[6] * m[2] + t[9 ] * m[3];
	out[1][2]  = t[3] * m[0] + t[6] * m[1] + t[11] * m[3];
	out[1][2] -= t[2] * m[0] + t[7] * m[1] + t[10] * m[3];
	out[1][3]  = t[4] * m[0] + t[9] * m[1] + t[10] * m[2];
	out[1][3] -= t[5] * m[0] + t[8] * m[1] + t[11] * m[2];

	t[0]  = m[2] * m[7];
	t[1]  = m[3] * m[6];
	t[2]  = m[1] * m[7];
	t[3]  = m[3] * m[5];
	t[4]  = m[1] * m[6];
	t[5]  = m[2] * m[5];
	t[6]  = m[0] * m[7];
	t[7]  = m[3] * m[4];
	t[8]  = m[0] * m[6];
	t[9]  = m[2] * m[4];
	t[10] = m[0] * m[5];
	t[11] = m[1] * m[4];

	out[2][0]  = t[0 ] * m[13] + t[3 ] * m[14] + t[ 4] * m[15];
	out[2][0] -= t[1 ] * m[13] + t[2 ] * m[14] + t[ 5] * m[15];
	out[2][1]  = t[1 ] * m[12] + t[6 ] * m[14] + t[ 9] * m[15];
	out[2][1] -= t[0 ] * m[12] + t[7 ] * m[14] + t[ 8] * m[15];
	out[2][2]  = t[2 ] * m[12] + t[7 ] * m[13] + t[10] * m[15];
	out[2][2] -= t[3 ] * m[12] + t[6 ] * m[13] + t[11] * m[15];
	out[2][3]  = t[5 ] * m[12] + t[8 ] * m[13] + t[11] * m[14];
	out[2][3] -= t[4 ] * m[12] + t[9 ] * m[13] + t[10] * m[14];
	out[3][0]  = t[2 ] * m[10] + t[5 ] * m[11] + t[ 1] * m[ 9];
	out[3][0] -= t[4 ] * m[11] + t[0 ] * m[ 9] + t[ 3] * m[10];
	out[3][1]  = t[8 ] * m[11] + t[0 ] * m[ 8] + t[ 7] * m[10];
	out[3][1] -= t[6 ] * m[10] + t[9 ] * m[11] + t[ 1] * m[ 8];
	out[3][2]  = t[6 ] * m[9 ] + t[11] * m[11] + t[ 3] * m[ 8];
	out[3][2] -= t[10] * m[11] + t[2 ] * m[ 8] + t[ 7] * m[ 9];
	out[3][3]  = t[10] * m[10] + t[4 ] * m[ 8] + t[ 9] * m[ 9];
	out[3][3] -= t[8 ] * m[9 ] + t[11] * m[10] + t[ 5] * m[ 8];

	float iDet = 1.0f / (m[0] * out[0][0] + m[1] * out[0][1] + m[2] * out[0][2] + m[3] * out[0][3]);
	for(size_t i = 0; i < 4; i++)
		for(size_t j = 0; j < 4; j++)
			out[i][j] *= iDet;

	return out;
}

const Matrix4 transpose(const Matrix4 &m) {
	return Matrix4(
			   float4(m[0][0], m[1][0], m[2][0], m[3][0]),
			   float4(m[0][1], m[1][1], m[2][1], m[3][1]),
			   float4(m[0][2], m[1][2], m[2][2], m[3][2]),
			   float4(m[0][3], m[1][3], m[2][3], m[3][3]));
}

const Matrix4 translation(const float3 &v) {
	return Matrix4(
		   float4(1.0f, 0.0f, 0.0f, 0.0f),
		   float4(0.0f, 1.0f, 0.0f, 0.0f),
		   float4(0.0f, 0.0f, 1.0f, 0.0f),
		   float4(v[0], v[1], v[2], 1.0f) );
}

const Matrix4 lookAt(const float3 &eye, const float3 &target, const float3 &up) {
	float3 front = normalize(target - eye);
	float3 side = normalize(cross(front, up));
	return transpose(side, cross(side, front), -front) * translation(-eye);
}

const Matrix4 perspective(float fov, float aspect, float zNear, float zFar) {
	float rad = fov * 0.5f;
	float deltaZ = zFar - zNear;

	float sin = ::sin(rad);

	Matrix4 out = identity();
	if(deltaZ != 0.0f && sin != 0.0f && aspect != 0.0f) {
		float ctg = cos(rad) / sin;
		out[0][0] = ctg / aspect;
		out[1][1] = ctg;
		out[2][2] = -(zFar + zNear) / deltaZ;
		out[2][3] = -1.0f;
		out[3][2] = -2.0f * zNear * zFar / deltaZ;
		out[3][3] = 0.0f;
	}
	return out;
}

const Matrix4 ortho(float left, float right, float top, float bottom, float near, float far) {
	Matrix4 out = identity();
	float ix = 1.0f / (right - left);
	float iy = 1.0f / (bottom - top);
	float iz = 1.0f / (far - near);

	out(0, 0) =  2.0f * ix;
	out(1, 1) =  2.0f * iy;
	out(2, 2) = -2.0f * iz;
	out(0, 3) = -(right + left) * ix;
	out(1, 3) = -(bottom + top) * iy;
	out(2, 3) = -(far + near) * iz;

	return out;
}

}
