// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/affine_trans.h"

#include "fwk/math/matrix4.h"
#include "fwk/sys/xml.h"

namespace fwk {

AffineTrans::AffineTrans(const Matrix4 &mat) {
	// Source: GLM
	float3 col[3] = {mat[0].xyz(), mat[1].xyz(), mat[2].xyz()};
	float3 skew;

	scale.x = length(col[0]);
	col[0] /= scale.x;

	// Compute XY shear factor and make 2nd col orthogonal to 1st.
	skew.z = dot(col[0], col[1]);
	col[1] -= col[0] * skew.z;

	scale.y = length(col[1]);
	col[1] /= scale.y;
	skew.z /= scale.y;

	// Compute XZ and YZ shears, orthogonalize 3rd col.
	skew.y = dot(col[0], col[2]);
	col[2] -= col[0] * skew.y;
	skew.x = dot(col[1], col[2]);
	col[2] -= col[1] * skew.x;

	// Next, get Z scale and normalize 3rd col.
	scale.z = length(col[2]);
	col[2] /= scale.z;
	skew.y /= scale.z;
	skew.x /= scale.z;

	// At this point, the matrix (in cols[]) is orthonormal.
	// Check for a coordinate system flip.  If the determinant
	// is -1, then negate the matrix and the scaling factors.
	float3 pdum3 = cross(col[1], col[2]); // v3Cross(col[1], col[2], Pdum3);
	if(dot(col[0], pdum3) < 0) {
		scale = -scale;
		col[0] = -col[0];
		col[1] = -col[1];
		col[2] = -col[2];
	}

	translation = mat[3].xyz();
	rotation = normalize(Quat(Matrix3(col[0], col[1], col[2])));
}

AffineTrans::operator Matrix4() const {
	const Matrix3 rot_matrix = Matrix3(normalize(rotation)) * Matrix3::scaling(scale);
	return Matrix4(float4(rot_matrix[0], 0.0f), float4(rot_matrix[1], 0.0f),
				   float4(rot_matrix[2], 0.0f), float4(translation, 1.0f));
}

AffineTrans operator*(const AffineTrans &lhs, const AffineTrans &rhs) {
	return AffineTrans(Matrix4(lhs) * Matrix4(rhs));
}

Ex<AffineTrans> AffineTrans::load(CXmlNode node) {
	return AffineTrans{node("translation", float3()), node("scale", float3(1)),
					   node("rotation", Quat())};
}

void AffineTrans::save(XmlNode node) const {
	node("translation", float3()) = translation;
	node("scale", float3(1)) = scale;
	node("rotation", Quat()) = rotation;
}

AffineTrans lerp(const AffineTrans &a, const AffineTrans &b, float t) {
	return AffineTrans(lerp(a.translation, b.translation, t), lerp(a.scale, b.scale, t),
					   slerp(a.rotation, b.rotation, t));
}
}
