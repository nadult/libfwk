/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

Quat::Quat(const Matrix3 &mat) {
	v[3] = sqrtf(max(0.0f, 1.0f + mat[0][0] + mat[1][1] + mat[2][2])) * 0.5f;
	v[0] = sqrtf(max(0.0f, 1.0f + mat[0][0] - mat[1][1] - mat[2][2])) * 0.5f;
	v[1] = sqrtf(max(0.0f, 1.0f - mat[0][0] + mat[1][1] - mat[2][2])) * 0.5f;
	v[2] = sqrtf(max(0.0f, 1.0f - mat[0][0] - mat[1][1] + mat[2][2])) * 0.5f;

	if(mat[2][1] > mat[1][2])
		v[0] = -v[0];
	if(mat[0][2] > mat[2][0])
		v[1] = -v[1];
	if(mat[1][0] > mat[0][1])
		v[2] = -v[2];
}

Quat::operator Matrix3() const {
	float tx = v[0] * 1.414213562373f;
	float ty = v[1] * 1.414213562373f;
	float tz = v[2] * 1.414213562373f;
	float tw = v[3] * 1.414213562373f;

	float xx = tx * tx, yy = ty * ty, zz = tz * tz;
	float xz = tx * tz, xy = tx * ty, yz = ty * tz;
	float xw = tx * tw, yw = ty * tw, zw = tz * tw;

	return Matrix3({1.0f - (yy + zz), (xy + zw), (xz - yw)},
				   {(xy - zw), 1.0f - (xx + zz), (yz + xw)},
				   {(xz + yw), (yz - xw), 1.0f - (xx + yy)});
}

Quat::Quat(float y, float p, float r) {
	float cy, cp, cr, sy, sp, sr;
	sincosf(y * 0.5f, &cy, &sy);
	sincosf(p * 0.5f, &cp, &sp);
	sincosf(r * 0.5f, &cr, &sr);

	v[0] = cy * cp * sr - sy * sp * cr;
	v[1] = cy * sp * cr + sy * cp * sr;
	v[2] = sy * cp * cr - cy * sp * sr;
	v[3] = cy * cp * cr + sy * sp * sr;
}

Quat::Quat(const AxisAngle &aa) {
	float s, c;
	sincosf(0.5f * aa.angle(), &s, &c);
	*this = normalize(Quat(float4(s * aa.axis()[0], s * aa.axis()[1], s * aa.axis()[2], c)));
}

Quat::operator AxisAngle() const {
	float sqrLen = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	return sqrLen > 0 ? AxisAngle(float3(v[0], v[1], v[2]) / sqrLen, 2.0f * acos(v[3]))
					  : AxisAngle(float3(1.0f, 0.0f, 0.0f), 0.0f);
}

const Quat Quat::operator*(const Quat &q) const {
	return Quat(float4(v[3] * q[0] + v[0] * q[3] + v[1] * q[2] - q[1] * v[2],
					   v[3] * q[1] + v[1] * q[3] + v[2] * q[0] - q[2] * v[0],
					   v[3] * q[2] + v[2] * q[3] + v[0] * q[1] - q[0] * v[1],
					   v[3] * q[3] - v[0] * q[0] - v[1] * q[1] - q[2] * v[2]));
}

const Quat inverse(const Quat &q) { return Quat(float4(-q[0], -q[1], -q[2], q[3])); }

const Quat normalize(const Quat &q) { return Quat(float4(q) / sqrtf(dot(q, q))); }

const Quat slerp(const Quat &lhs, const Quat &rhs, float t) {
	float angle = acos(clamp(dot(lhs, rhs), -1.0f, 1.0f));

	if(fabsf(angle) < constant::epsilon)
		return lhs;

	float inv_sin = 1.0f / sin(angle);
	float coeff0 = sin((1.0f - t) * angle) * inv_sin;
	float coeff1 = sin(t * angle) * inv_sin;

	return normalize(lhs * coeff0 + rhs * coeff1);
}
}
