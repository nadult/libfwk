// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/matrix4.h"

#include "fwk/format.h"

// TODO: make it work on html
#ifndef FWK_PLATFORM_HTML
#include <emmintrin.h>
#endif

#include <cmath>

namespace fwk {

Matrix4 Matrix4::identity() {
	return Matrix4({1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f},
				   {0.0f, 0.0f, 0.0f, 1.0f});
}

Matrix4 Matrix4::zero() { return Matrix4(float4(), float4(), float4(), float4()); }

Matrix4 Matrix4::operator+(const Matrix4 &rhs) const {
	Matrix4 out;
	for(int n = 0; n < 4; n++)
		out[n] = v[n] + rhs[n];
	return out;
}

Matrix4 Matrix4::operator-(const Matrix4 &rhs) const {
	Matrix4 out;
	for(int n = 0; n < 4; n++)
		out[n] = v[n] + rhs[n];
	return out;
}

Matrix4 Matrix4::operator*(float s) const {
	Matrix4 out;
	for(int n = 0; n < 4; n++)
		out[n] = v[n] * s;
	return out;
}

Matrix4 Matrix4::operator*(const Matrix4 &rhs) const {
	// This is vectorized by default
	Matrix4 tlhs = transpose(*this);

	return Matrix4(float4(dot(rhs[0], tlhs[0]), dot(rhs[0], tlhs[1]), dot(rhs[0], tlhs[2]),
						  dot(rhs[0], tlhs[3])),
				   float4(dot(rhs[1], tlhs[0]), dot(rhs[1], tlhs[1]), dot(rhs[1], tlhs[2]),
						  dot(rhs[1], tlhs[3])),
				   float4(dot(rhs[2], tlhs[0]), dot(rhs[2], tlhs[1]), dot(rhs[2], tlhs[2]),
						  dot(rhs[2], tlhs[3])),
				   float4(dot(rhs[3], tlhs[0]), dot(rhs[3], tlhs[1]), dot(rhs[3], tlhs[2]),
						  dot(rhs[3], tlhs[3])));
}

float4 Matrix4::operator*(const float4 &vector) const {
	return float4(dot(row(0), vector), dot(row(1), vector), dot(row(2), vector),
				  dot(row(3), vector));
}

float3 mulPoint(const Matrix4 &mat, const float3 &pt) {
#ifdef __SSE2__
	auto r0 = _mm_loadu_ps(mat[0].v);
	auto r1 = _mm_loadu_ps(mat[1].v);
	auto r2 = _mm_loadu_ps(mat[2].v);
	auto r3 = _mm_loadu_ps(mat[3].v);

	alignas(16) float out[4];
	__m128 result = _mm_add_ps(
		_mm_add_ps(_mm_mul_ps(r0, _mm_set1_ps(pt[0])), _mm_mul_ps(r1, _mm_set1_ps(pt[1]))),
		_mm_add_ps(_mm_mul_ps(r2, _mm_set1_ps(pt[2])), _mm_mul_ps(r3, _mm_set1_ps(1.0f))));
	_mm_store_ps(&out[0], result);
	return float3(out[0], out[1], out[2]) / out[3];
#else
	float4 tmp = mat * float4(pt, 1.0f);
	return tmp.xyz() / tmp.w;
#endif
}

float3 mulPointAffine(const Matrix4 &affine_mat, const float3 &pt) {
	return float3(dot(affine_mat.row(0).xyz(), pt), dot(affine_mat.row(1).xyz(), pt),
				  dot(affine_mat.row(2).xyz(), pt)) +
		   affine_mat[3].xyz();
}

float3 mulNormal(const Matrix4 &inverse_transpose, const float3 &nrm) {
	float4 tmp = inverse_transpose * float4(nrm, 0.0f);
	return tmp.xyz();
}

float3 mulNormalAffine(const Matrix4 &affine_mat, const float3 &nrm) {
	return float3(dot(affine_mat.row(0).xyz(), nrm), dot(affine_mat.row(1).xyz(), nrm),
				  dot(affine_mat.row(2).xyz(), nrm));
}

Matrix4 transpose(const float4 &a, const float4 &b, const float4 &c, const float4 &d) {
	return Matrix4({a[0], b[0], c[0], d[0]}, {a[1], b[1], c[1], d[1]}, {a[2], b[2], c[2], d[2]},
				   {a[3], b[3], c[3], d[3]});
}

bool Matrix4::invert(const Matrix4 &rhs) {
	float t[12], m[16];

	for(size_t n = 0; n < 4; n++) {
		m[n + 0] = rhs[n][0];
		m[n + 4] = rhs[n][1];
		m[n + 8] = rhs[n][2];
		m[n + 12] = rhs[n][3];
	}

	t[0] = m[10] * m[15];
	t[1] = m[11] * m[14];
	t[2] = m[9] * m[15];
	t[3] = m[11] * m[13];
	t[4] = m[9] * m[14];
	t[5] = m[10] * m[13];
	t[6] = m[8] * m[15];
	t[7] = m[11] * m[12];
	t[8] = m[8] * m[14];
	t[9] = m[10] * m[12];
	t[10] = m[8] * m[13];
	t[11] = m[9] * m[12];

	v[0][0] = t[0] * m[5] + t[3] * m[6] + t[4] * m[7];
	v[0][0] -= t[1] * m[5] + t[2] * m[6] + t[5] * m[7];
	v[0][1] = t[1] * m[4] + t[6] * m[6] + t[9] * m[7];
	v[0][1] -= t[0] * m[4] + t[7] * m[6] + t[8] * m[7];
	v[0][2] = t[2] * m[4] + t[7] * m[5] + t[10] * m[7];
	v[0][2] -= t[3] * m[4] + t[6] * m[5] + t[11] * m[7];
	v[0][3] = t[5] * m[4] + t[8] * m[5] + t[11] * m[6];
	v[0][3] -= t[4] * m[4] + t[9] * m[5] + t[10] * m[6];
	v[1][0] = t[1] * m[1] + t[2] * m[2] + t[5] * m[3];
	v[1][0] -= t[0] * m[1] + t[3] * m[2] + t[4] * m[3];
	v[1][1] = t[0] * m[0] + t[7] * m[2] + t[8] * m[3];
	v[1][1] -= t[1] * m[0] + t[6] * m[2] + t[9] * m[3];
	v[1][2] = t[3] * m[0] + t[6] * m[1] + t[11] * m[3];
	v[1][2] -= t[2] * m[0] + t[7] * m[1] + t[10] * m[3];
	v[1][3] = t[4] * m[0] + t[9] * m[1] + t[10] * m[2];
	v[1][3] -= t[5] * m[0] + t[8] * m[1] + t[11] * m[2];

	t[0] = m[2] * m[7];
	t[1] = m[3] * m[6];
	t[2] = m[1] * m[7];
	t[3] = m[3] * m[5];
	t[4] = m[1] * m[6];
	t[5] = m[2] * m[5];
	t[6] = m[0] * m[7];
	t[7] = m[3] * m[4];
	t[8] = m[0] * m[6];
	t[9] = m[2] * m[4];
	t[10] = m[0] * m[5];
	t[11] = m[1] * m[4];

	v[2][0] = t[0] * m[13] + t[3] * m[14] + t[4] * m[15];
	v[2][0] -= t[1] * m[13] + t[2] * m[14] + t[5] * m[15];
	v[2][1] = t[1] * m[12] + t[6] * m[14] + t[9] * m[15];
	v[2][1] -= t[0] * m[12] + t[7] * m[14] + t[8] * m[15];
	v[2][2] = t[2] * m[12] + t[7] * m[13] + t[10] * m[15];
	v[2][2] -= t[3] * m[12] + t[6] * m[13] + t[11] * m[15];
	v[2][3] = t[5] * m[12] + t[8] * m[13] + t[11] * m[14];
	v[2][3] -= t[4] * m[12] + t[9] * m[13] + t[10] * m[14];
	v[3][0] = t[2] * m[10] + t[5] * m[11] + t[1] * m[9];
	v[3][0] -= t[4] * m[11] + t[0] * m[9] + t[3] * m[10];
	v[3][1] = t[8] * m[11] + t[0] * m[8] + t[7] * m[10];
	v[3][1] -= t[6] * m[10] + t[9] * m[11] + t[1] * m[8];
	v[3][2] = t[6] * m[9] + t[11] * m[11] + t[3] * m[8];
	v[3][2] -= t[10] * m[11] + t[2] * m[8] + t[7] * m[9];
	v[3][3] = t[10] * m[10] + t[4] * m[8] + t[9] * m[9];
	v[3][3] -= t[8] * m[9] + t[11] * m[10] + t[5] * m[8];

	float det = m[0] * v[0][0] + m[1] * v[0][1] + m[2] * v[0][2] + m[3] * v[0][3];
	if(det == 0.0f)
		return false;

	float inv_det = 1.0f / det;
	for(size_t i = 0; i < 4; i++)
		for(size_t j = 0; j < 4; j++)
			v[i][j] *= inv_det;

	return true;
}

Matrix4 inverseOrZero(const Matrix4 &mat) {
	Matrix4 out;
	if(!out.invert(mat))
		out[0] = out[1] = out[2] = out[3] = float4();
	return out;
}

Matrix4 transpose(const Matrix4 &m) {
	return Matrix4(
		float4(m[0][0], m[1][0], m[2][0], m[3][0]), float4(m[0][1], m[1][1], m[2][1], m[3][1]),
		float4(m[0][2], m[1][2], m[2][2], m[3][2]), float4(m[0][3], m[1][3], m[2][3], m[3][3]));
}

Matrix4 translation(const float3 &v) {
	return Matrix4(float4(1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f),
				   float4(0.0f, 0.0f, 1.0f, 0.0f), float4(v[0], v[1], v[2], 1.0f));
}

Matrix4 lookAt(const float3 &eye, const float3 &target, const float3 &up) {
	float3 front = normalize(target - eye);
	float3 side = normalize(cross(front, up));
	return Matrix4(transpose(side, cross(side, front), -front)) * translation(-eye);
}

Matrix4 perspective(float vert_fov_rad, float aspect_ratio, float z_near, float z_far) {
	DASSERT(vert_fov_rad > 0.0f && vert_fov_rad < pi);
	DASSERT(aspect_ratio > 0.0f);
	DASSERT(z_near >= 0.0f && z_far > z_near);
	DASSERT(std::isfinite(z_far));

	float ctg = 1.0f / std::tan(0.5f * vert_fov_rad);
	float z_diff = z_far - z_near;

	Matrix4 out = Matrix4::zero();
	out[0][0] = ctg / aspect_ratio;
	out[1][1] = -ctg;
	out[2][2] = -(z_far + z_near) / z_diff;
	out[2][3] = -1.0f;
	out[3][2] = -(2.0f * z_near * z_far) / z_diff;
	return out;
}

Matrix4 ortho(float left, float right, float top, float bottom, float near, float far) {
	Matrix4 out = Matrix4::identity();
	float ix = 1.0f / (right - left);
	float iy = 1.0f / (top - bottom);
	float iz = 1.0f / (far - near);

	out(0, 0) = 2.0f * ix;
	out(1, 1) = -2.0f * iy;
	out(2, 2) = -1.0f * iz;
	out(0, 3) = -(right + left) * ix;
	out(1, 3) = (top + bottom) * iy;
	out(2, 3) = -near * iz;
	return out;
}

Matrix4 projectionMatrix2D(const IRect &viewport, Orient2D orient, Interval<float> depth) {
	int y_min = orient == Orient2D::y_up ? viewport.ey() : viewport.y();
	int y_max = orient == Orient2D::y_up ? viewport.y() : viewport.ey();
	return ortho(viewport.x(), viewport.ex(), y_min, y_max, depth.min, depth.max);
}

void Matrix4::operator>>(TextFormatter &out) const {
	out(out.isStructured() ? "(%; %; %; %)" : "% % % %", v[0], v[1], v[2], v[3]);
}
}
