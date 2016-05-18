/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include "fwk_xml.h"

namespace fwk {

Quat::Quat(const Matrix3 &mat) {
	// Source: GLM
	float fourXSquaredMinus1 = mat[0][0] - mat[1][1] - mat[2][2];
	float fourYSquaredMinus1 = mat[1][1] - mat[0][0] - mat[2][2];
	float fourZSquaredMinus1 = mat[2][2] - mat[0][0] - mat[1][1];
	float fourWSquaredMinus1 = mat[0][0] + mat[1][1] + mat[2][2];

	int biggestIndex = 0;
	float fourBiggestSquaredMinus1 = fourWSquaredMinus1;
	if(fourXSquaredMinus1 > fourBiggestSquaredMinus1) {
		fourBiggestSquaredMinus1 = fourXSquaredMinus1;
		biggestIndex = 1;
	}
	if(fourYSquaredMinus1 > fourBiggestSquaredMinus1) {
		fourBiggestSquaredMinus1 = fourYSquaredMinus1;
		biggestIndex = 2;
	}
	if(fourZSquaredMinus1 > fourBiggestSquaredMinus1) {
		fourBiggestSquaredMinus1 = fourZSquaredMinus1;
		biggestIndex = 3;
	}

	float biggestVal = sqrt(fourBiggestSquaredMinus1 + float(1)) * float(0.5);
	float mult = 0.25 / biggestVal;

	switch(biggestIndex) {
	case 0:
		*this = Quat((mat[1][2] - mat[2][1]) * mult, (mat[2][0] - mat[0][2]) * mult,
					 (mat[0][1] - mat[1][0]) * mult, biggestVal);
		break;
	case 1:

		*this = Quat(biggestVal, (mat[0][1] + mat[1][0]) * mult, (mat[2][0] + mat[0][2]) * mult,
					 (mat[1][2] - mat[2][1]) * mult);
		break;
	case 2:
		*this = Quat((mat[0][1] + mat[1][0]) * mult, biggestVal, (mat[1][2] + mat[2][1]) * mult,
					 (mat[2][0] - mat[0][2]) * mult);
		break;
	case 3:
		*this = Quat((mat[2][0] + mat[0][2]) * mult, (mat[1][2] + mat[2][1]) * mult, biggestVal,
					 (mat[0][1] - mat[1][0]) * mult);
		break;
	}
}

Quat::operator Matrix3() const {
	// Source: GLM
	Matrix3 out;
	float qxx(x * x);
	float qyy(y * y);
	float qzz(z * z);
	float qxz(x * z);
	float qxy(x * y);
	float qyz(y * z);
	float qwx(w * x);
	float qwy(w * y);
	float qwz(w * z);
	float mul = 2.0f / length(*this);

	out[0][0] = 1 - mul * (qyy + qzz);
	out[0][1] = mul * (qxy + qwz);
	out[0][2] = mul * (qxz - qwy);

	out[1][0] = mul * (qxy - qwz);
	out[1][1] = 1 - mul * (qxx + qzz);
	out[1][2] = mul * (qyz + qwx);

	out[2][0] = mul * (qxz + qwy);
	out[2][1] = mul * (qyz - qwx);
	out[2][2] = 1 - mul * (qxx + qyy);
	return out;
}

#ifdef FWK_TARGET_MSVC
static void sincosf(float deg, float *s, float *c) {
	*s = sinf(deg);
	*c = cosf(deg);
}
#endif

const Quat Quat::fromYawPitchRoll(float y, float p, float r) {
	float cy, cp, cr, sy, sp, sr;
	sincosf(y * 0.5f, &cy, &sy); // TODO: swap c&s?
	sincosf(p * 0.5f, &cp, &sp);
	sincosf(r * 0.5f, &cr, &sr);

	return Quat(cy * cp * sr - sy * sp * cr, cy * sp * cr + sy * cp * sr,
				sy * cp * cr - cy * sp * sr, cy * cp * cr + sy * sp * sr);
}

Quat::Quat(const AxisAngle &aa) {
	float s, c;
	sincosf(0.5f * aa.angle(), &s, &c);
	*this = normalize(Quat(float4(s * aa.axis()[0], s * aa.axis()[1], s * aa.axis()[2], c)));
}

Quat::operator AxisAngle() const {
	float sqrLen = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	return sqrLen > 0 ? AxisAngle(float3(v[0], v[1], v[2]) / sqrLen, 2.0f * acos(v[3]))
					  : AxisAngle(float3(0.0f, 1.0f, 0.0f), 0.0f);
}

const Quat Quat::operator*(const Quat &q) const {
	return Quat(float4(v[3] * q[0] + v[0] * q[3] + v[1] * q[2] - q[1] * v[2],
					   v[3] * q[1] + v[1] * q[3] + v[2] * q[0] - q[2] * v[0],
					   v[3] * q[2] + v[2] * q[3] + v[0] * q[1] - q[0] * v[1],
					   v[3] * q[3] - v[0] * q[0] - v[1] * q[1] - q[2] * v[2]));
}

const Quat inverse(const Quat &q) { return conjugate(q) * (1.0f / dot(q, q)); }

const Quat normalize(const Quat &q) { return Quat(float4(q) / sqrtf(dot(q, q))); }

const Quat slerp(const Quat &lhs, Quat rhs, float t) {
	float qdot = dot(lhs, rhs);
	if(qdot < 0.0f) {
		qdot = -qdot;
		rhs = -rhs;
	}

	float coeff0, coeff1;

	if(1.0f - qdot > constant::epsilon) {
		float angle = acos(qdot);
		float inv_sin = 1.0f / sin(angle);
		coeff0 = sin((1.0f - t) * angle) * inv_sin;
		coeff1 = sin(t * angle) * inv_sin;
	} else {
		coeff0 = 1.0f - t;
		coeff1 = t;
	}

	return normalize(lhs * coeff0 + rhs * coeff1);
}

float distance(const Quat &lhs, const Quat &rhs) { return 2.0f * (1.0f - dot(lhs, rhs)); }

const Quat rotationBetween(const float3 &v1, const float3 &v2) {
	return normalize(Quat(cross(v1, v2), sqrt(lengthSq(v1) * lengthSq(v2)) + dot(v1, v2)));
}

const Quat conjugate(const Quat &q) { return Quat(-q.xyz(), q.w); }

/*
 //TODO: fixme
const Quat exp(const Quat &q) {
	float r = length(q.xyz());
	float s = r > constant::epsilon ? sin(r) / r : 1.0f;

	return Quat(q.xyz() * s, cosf(r));
}

const Quat log(const Quat &q) {
	float r = length(q.xyz());
	float s = atanf(r / q.w);
	return Quat(q.xyz() * s, 0.0f);
}

const Quat pow(const Quat &q, float value) { return exp(normalize(log(q)) * value); }*/
}
