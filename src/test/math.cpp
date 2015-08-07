/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "testing.h"

float3 randomTranslation(float magnitude) {
	return float3(frand() - 0.5f, frand() - 0.5f, frand() - 0.5f) * 2.0f * magnitude;
}

float3 randomScale() {
	return float3(1.0f + frand() * 2.0f, 1.0f + frand() * 2.0f, frand() * 2.0f + 1.0f);
}

Quat randomRotation() {
	return normalize(Quat(AxisAngle(
		normalize(float3(frand() * 2.0f - 1.0f, frand() * 2.0f - 1.0f, frand() * 2.0f - 1.0f)),
		frand() * constant::pi * 2.0f)));
}

AffineTrans randomTransform() {
	return AffineTrans(randomTranslation(50.0f), randomScale(), randomRotation());
}

void testMatrices() {
	float3 up(0, 1, 0);
	float angle = constant::pi * 0.25f;

	float3 rot_a = mulNormal(rotation(up, angle), float3(1, 0, 0));
	float3 rot_b = mulNormal(rotation(up, angle), float3(0, 0, 1));

	for(int n = 0; n < 100; n++) {
		float3 trans = randomTranslation(100.0f);
		float3 scale = randomScale();
		Quat rot = randomRotation();

		Matrix4 mat = translation(trans) * Matrix4(rot) * scaling(scale);
		AffineTrans dec(mat);
		assertCloseEnough(trans, dec.translation);
		assertCloseEnough(scale, dec.scale);
	}

	for(int n = 0; n < 100; n++) {
		AffineTrans trans1 = randomTransform(), trans2 = randomTransform();
		Matrix4 mtrans1(trans1), mtrans2(trans2);

		AffineTrans result0 = AffineTrans(mtrans1);

		AffineTrans result1 = trans1 * trans2;
		AffineTrans result2 = AffineTrans(mtrans1 * mtrans2);

		assertCloseEnough(result0.translation, trans1.translation);
		assertCloseEnough(result0.scale, trans1.scale);
		assertCloseEnough(result0.rotation, trans1.rotation);

		assertCloseEnough(result1.translation, result2.translation);
		assertCloseEnough(result1.scale, result2.scale);
		assertCloseEnough(result1.rotation, result2.rotation);
	}

	// TODO: finish me
}

void testRays() {
	Triangle tri(float3(0, 0, 4), float3(0, 2, 4), float3(2, 0, 4));
	Segment segment1(float3(0.5, 0.5, 0), float3(0.5, 0.5, 10));
	Segment segment2(float3(1.3, 1.3, 0), float3(1.0, 1.0, 10));

	assertCloseEnough(intersection(segment1, tri), 4.0f);
	assertEqual(intersection(segment2, tri), constant::inf);
	assertCloseEnough(tri.area(), 2.0f);
}

void testMain() {
	FBox box(0, -100, 0, 1200, 100, 720);
	FBox temp(32, 0, 32, 64, 0.5f, 64);
	ASSERT(areOverlapping(box, temp));

	testMatrices();
	testRays();

	float3 vec(0, 0, 1);

	/*
	Quat rot = normalize(Quat::fromYawPitchRoll(0.5, 1.2, 0.3));
	Quat p = normalize(pow(rot, 1.0f));

	float3 v1 = mulNormal((Matrix4)rot, vec);
	float3 v2 = mulNormal((Matrix4)p, vec);

	printf("result:\n%f %f %f %f\n%f %f %f %f\n", rot.x, rot.y, rot.z, rot.w, p.x, p.y, p.z,
	p.w);
	printf("result:\n%f %f %f\n%f %f %f\n", v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);*/
}
