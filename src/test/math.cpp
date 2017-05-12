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
		frand() * fconstant::pi * 2.0f)));
}

AffineTrans randomTransform() {
	return AffineTrans(randomTranslation(50.0f), randomRotation(), randomScale());
}

void testMatrices() {
	float3 up(0, 1, 0);
	float angle = fconstant::pi * 0.25f;

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
	assertEqual(intersection(segment2, tri), fconstant::inf);
	assertCloseEnough(tri.surfaceArea(), 2.0f);

	Segment segment3(float3(1, 1, 0), float3(4, 4, 0));
	float3 p1(4, 1, 0), p2(0.5, 0.5, 0), p3(5, 4, 0);
	assertCloseEnough(closestPoint(segment3, p1), float3(2.5, 2.5, 0));
	assertCloseEnough(closestPoint(segment3, p2), float3(1, 1, 0));
	assertCloseEnough(closestPoint(segment3, p3), float3(4, 4, 0));

	assertCloseEnough(closestPoint(Ray(segment3), p1), float3(2.5, 2.5, 0));
	assertCloseEnough(closestPoint(Ray(segment3), p2), float3(0.5, 0.5, 0));
	assertCloseEnough(closestPoint(Ray(segment3), p3), float3(4.5, 4.5, 0));

	Segment segment4(float3(3, 2, 0), float3(6, 5, 0));
	Segment segment5(float3(6, 7, 0), float3(8, 5, 0));
	assertCloseEnough(distance(segment3, segment4), sqrtf(2.0f) / 2.0f);
	assertCloseEnough(distance(segment4, segment5), sqrtf(2.0f));
	assertCloseEnough(distance(Ray(segment4), Ray(segment5)), 0.0f);
}

void testIntersections() {
	assertEqual(distance(Cylinder(float3(1, 2, 3), 0.5, 2.0), float3(2, 2, 3)), 0.5f);
	assertEqual(distance(Cylinder(float3(1, 1, 1), 1.5, 2.0), float3(2, 1, 1)), 0.0f);
	assertEqual(distance(Cylinder(float3(2, 2, 2), 1.5, 2.0), float3(2, 5, 2)), 1.0f);

	Triangle tri(float3(0, 0, 0), float3(1, 0, 0), float3(0, 1, 0));
	Segment seg(float3(1, 1, -1), float3(1, 1, 1));

	assertEqual(intersection(tri, seg), fconstant::inf);
	assertEqual(distance(tri, float3(1, 1, 0)), sqrtf(2.0f) / 2.0f);
	assertEqual(distance(tri, seg), sqrtf(2.0f) / 2.0f);

	/*
	Triangle tri1(float3(4.330130, 10.000000, 2.500000), float3(-4.330130, 10.000000, 2.500000),
				  float3(0.000000, 1.730360, 0.000000));
	Triangle tri2(float3(-2.500000, 0.000000, -4.330130), float3(4.330130, 10.000000, 2.500000),
				  float3(-2.500000, 0.000000, 4.330130));
	ASSERT(areIntersecting(tri1, tri2));*/

	// TODO: 2d Segment intersections
	/*	Segment2D seg1(float2(-100, -100), float2(100, 100));
		Segment2D seg2(float2(-100, -99), float2(100, 99));
		auto isect = intersection(seg1, seg2);
		xmlPrint("% %\n", isect.first, isect.second);*/

	Tetrahedron tet({0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0.25, 1, 0.25});
	assertEqual(tet.volume(), 1.0f / 6.0f);

	FBox bbox1({0, 0, 0}, {1, 1, 1});
	FBox bbox2({0.49, 0, 0.49}, {1, 1, 1});
	FBox bbox3({0.45, 0.5, 0.45}, {2, 2, 2});

	//	assertEqual(areIntersecting(tet, bbox1), true);
	//	assertEqual(areIntersecting(tet, bbox2), true);
	//	assertEqual(areIntersecting(tet, bbox3), false);

	// TODO: fix sat
	/*	for(int n = 0; n < 10000; n++) {
			float3 points[4];
			for(int i = 0; i < 4; i++)
				points[i] = float3(int3(randomTranslation(100.0f)));

			FBox box1(makeRange({points[0], points[1]}));
			FBox box2(makeRange({points[2], points[3]}));
			try {
				assertEqual(areOverlapping(box1, box2), satTest(box1, box2));
			} catch(const Exception &ex) {
				xmlPrint("Box1: %\nBox2: %\n", box1, box2);
				throw;
			}
		}*/
}

void test2DIntersections() {
	Triangle2D tri(float2(1, 2), float2(5, 3), float2(3, 5));
	Segment2D seg(float2(3, 1), float2(3, 6));
	auto result = clip(tri, seg);
	assertEqual(result.inside.start, float2(3, 2.5f));
	assertEqual(result.inside.end, float2(3, 5));
	assertEqual(result.outside_front.start, float2(3, 1));
	assertEqual(result.outside_front.end, float2(3, 2.5f));
	assertEqual(result.outside_back.start, float2(3, 5));
	assertEqual(result.outside_back.end, float2(3, 6));

	Segment2D seg1({0, 2}, {3, 2}), seg2({1, 1}, {1, 3}), seg3({1.00001, 3}, {3, 3});
	assertEqual(intersection(seg1, seg2).first, float2(1, 2));
	ASSERT(!intersection(seg1, seg3).second);
	ASSERT(!intersection(seg2, seg3).second);

	Segment2D seg4({-3.788882, -9.852953}, {-3.870192, -10.849642});
	Segment2D seg5({-3.723713, -9.857805}, {-3.771331, -9.854259});
	ASSERT(!intersection(seg4, seg5).second);
	ASSERT(!intersection(seg5, seg4).second);
}

static_assert(isVector<short2>(), "");
static_assert(isVector<float4>(), "");
static_assert(!isVector<vector<int>>(), "");

static_assert(isRealObject<FRect>(), "");
static_assert(isIntegralObject<IBox>(), "");
static_assert(isIntegralVector<int3>(), "");

void testMain() {
	FBox box(0, -100, 0, 1200, 100, 720);
	FBox temp(32, 0, 32, 64, 0.5f, 64);
	ASSERT(areOverlapping(box, temp));

	testMatrices();
	testRays();
	testIntersections();
	test2DIntersections();

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
