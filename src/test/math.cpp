/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_assert.h"
#include "fwk_variant.h"
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
	Triangle tri1(float3(0, 0, 4), float3(0, 2, 4), float3(2, 0, 4));
	Triangle tri2(float3(1, 0, 1), float3(6, 0, 1), float3(1, 0, 6));

	Segment3<float> segment1(0.5, 0.5, 0, 0.5, 0.5, 10);
	Segment3<float> segment2(1.3, 1.3, 0, 1.0, 1.0, 10);

	assertCloseEnough(intersection(segment1, tri1), 0.4f);
	ASSERT_EQ(intersection(segment2, tri1), fconstant::inf);
	assertCloseEnough(tri1.surfaceArea(), 2.0f);

	auto angles2 = tri2.angles();
	assertCloseEnough(float3(angles2[0], angles2[1], angles2[2]),
					  float3(0.5f, 0.25f, 0.25f) * fconstant::pi);

	Segment3<float> segment3(float3(1, 1, 0), float3(4, 4, 0));
	float3 p1(4, 1, 0), p2(0.5, 0.5, 0), p3(5, 4, 0);
	assertCloseEnough(segment3.closestPoint(p1), float3(2.5, 2.5, 0));
	assertCloseEnough(segment3.closestPoint(p2), float3(1, 1, 0));
	assertCloseEnough(segment3.closestPoint(p3), float3(4, 4, 0));

	auto ray = *segment3.asRay();
	assertCloseEnough(closestPoint(ray, p1), float3(2.5, 2.5, 0));
	assertCloseEnough(closestPoint(ray, p2), float3(0.5, 0.5, 0));
	assertCloseEnough(closestPoint(ray, p3), float3(4.5, 4.5, 0));

	Segment3<float> segment4({3, 2, 0}, {6, 5, 0});
	Segment3<float> segment5({6, 7, 0}, {8, 5, 0});
	auto ray4 = *segment4.asRay(), ray5 = *segment5.asRay();
	assertCloseEnough(segment3.distance(segment4), sqrtf(2.0f) / 2.0f);
	assertCloseEnough(segment4.distance(segment5), sqrtf(2.0f));
	assertCloseEnough(distance(ray4, ray5), 0.0f);
}

void testIntersections() {
	ASSERT_EQ(distance(Cylinder(float3(1, 2, 3), 0.5, 2.0), float3(2, 2, 3)), 0.5f);
	ASSERT_EQ(distance(Cylinder(float3(1, 1, 1), 1.5, 2.0), float3(2, 1, 1)), 0.0f);
	ASSERT_EQ(distance(Cylinder(float3(2, 2, 2), 1.5, 2.0), float3(2, 5, 2)), 1.0f);

	Triangle tri(float3(0, 0, 0), float3(1, 0, 0), float3(0, 1, 0));
	Segment3<float> seg(1, 1, -1, 1, 1, 1);

	ASSERT_EQ(intersection(tri, seg), fconstant::inf);
	ASSERT_EQ(distance(tri, float3(1, 1, 0)), sqrtf(2.0f) / 2.0f);
	ASSERT_EQ(distance(tri, seg), sqrtf(2.0f) / 2.0f);

	/*
	Triangle tri1(float3(4.330130, 10.000000, 2.500000), float3(-4.330130, 10.000000, 2.500000),
				  float3(0.000000, 1.730360, 0.000000));
	Triangle tri2(float3(-2.500000, 0.000000, -4.330130), float3(4.330130, 10.000000, 2.500000),
				  float3(-2.500000, 0.000000, 4.330130));
	ASSERT(areIntersecting(tri1, tri2));*/

	Tetrahedron tet({0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0.25, 1, 0.25});
	ASSERT_EQ(tet.volume(), 1.0f / 6.0f);

	FBox bbox1({0, 0, 0}, {1, 1, 1});
	FBox bbox2({0.49, 0, 0.49}, {1, 1, 1});
	FBox bbox3({0.45, 0.5, 0.45}, {2, 2, 2});

	//	ASSERT_EQ(areIntersecting(tet, bbox1), true);
	//	ASSERT_EQ(areIntersecting(tet, bbox2), true);
	//	ASSERT_EQ(areIntersecting(tet, bbox3), false);

	// TODO: fix sat
	/*	for(int n = 0; n < 10000; n++) {
			float3 points[4];
			for(int i = 0; i < 4; i++)
				points[i] = float3(int3(randomTranslation(100.0f)));

			FBox box1(makeRange({points[0], points[1]}));
			FBox box2(makeRange({points[2], points[3]}));
			try {
				ASSERT_EQ(areOverlapping(box1, box2), satTest(box1, box2));
			} catch(const Exception &ex) {
				xmlPrint("Box1: %\nBox2: %\n", box1, box2);
				throw;
			}
		}*/
}

template <class T> void print(const typename Segment<T, 2>::Isect &isect) {
	if(const Segment2<T> *seg = isect)
		xmlPrint("Segment(% - %)\n", seg->from, seg->to);
	else if(const Vector2<T> *vec = isect)
		xmlPrint("Vector %\n", *vec);
	else
		xmlPrint("Empty\n");
}

void test2DIntersections() {
	Segment2<float> s1(1, 4, 4, 1);
	Segment2<float> s2(3, 2, 5, 0);

	Segment2<double> s3(3, 2, 5, 0);
	Segment2<double> s4(1, 4, 4, 1);

	Segment2<double> s5(1, 7, 1, 4);
	Segment2<double> s6(-1, -1, 4, 4);

	//	print(intersection(s1, s2));
	//	print(intersection(s3, s4));
	//	print(intersection(s6, Segment2<double>({-1, -1}, {-1, -1})));

	ASSERT(s1.isect(s2) == Segment2<float>(3, 2, 4, 1));
	ASSERT(s3.isect(s4) == Segment2<double>(3, 2, 4, 1));
	ASSERT(s5.isect(s4) == double2(1, 4));
	ASSERT(s6.isect(s4) == double2(2.5, 2.5));
	ASSERT(s6.isect(Segment2<double>{4.1, 4.1, 5, 5}) == none);
	ASSERT(s4.isect(Segment2<double>{0, 3, 6, -1}) == none);
	ASSERT(s6.isect(Segment2<double>{-1, -1, -1, -1}) == double2(-1, -1));

	ASSERT_EQ(s6.closestPointParam({0.5, 2.5}), 0.5);

	Segment2<double> seg1(-5.6, -9.1, -4.2, -9.5);
	Segment2<double> seg2(-4.1, -9.4, -2.4, -9.2);
	ASSERT(seg1.isect(seg2) == none);

	using ISeg = ISegment2<int>;
	using IClass = SegmentIsectClass;

	ISeg iseg1(0, 0, 943782983, 999999999), iseg2(0, 1, 1000000123, 2);
	ISeg iseg3(-1, 0, 943782982, 999999999), iseg4(-123456789, 934567893, 985473892, -848372819);
	ASSERT(iseg1.classifyIsect(iseg2) == IClass::point);
	ASSERT(iseg1.classifyIsect(iseg3) == IClass::none);
	ASSERT(iseg1.classifyIsect(iseg4) == IClass::point);

	ASSERT(ISeg(0, 0, 10, 0).classifyIsect(ISeg(0, 0, 5, 0)) == IClass::segment);
	ASSERT(ISeg(0, 0, 10, 0).classifyIsect(ISeg(10, 0, 11, 0)) == IClass::shared_endpoints);
	ASSERT(ISeg(0, 0, 10, 0).classifyIsect(ISeg(-1, 0, 0, 0)) == IClass::shared_endpoints);
	ASSERT(ISeg(0, 0, 10, 0).classifyIsect(ISeg(0, 10, 0, 0)) == IClass::shared_endpoints);
	ASSERT(ISeg(0, 0, 2, 0).classifyIsect(int2(1, 0)) == IClass::point);
	ASSERT(ISeg(0, 0, 5, 5).classifyIsect(int2(3, 3)) == IClass::point);
	ASSERT(ISeg(0, 0, 5, 5).classifyIsect(int2(5, 5)) == IClass::shared_endpoints);
	ASSERT(ISeg(0, 0, 5, 5).classifyIsect(int2(2, 3)) == IClass::none);

	ISeg seg5(1, 1, 4, 4);
	ASSERT(seg5.classifyIsect(ISeg(3, 3, 3, 3)) == IClass::point);

	auto time = getTime();
	for(int n = 0; n < 50000; n++) {
		s3.isect(s4);
		s6.isect(s4);
	}
	time = getTime() - time;
	xmlPrint("Isect time: % ns / Segment<double> pair\n", time * 10000);

	time = getTime();
	for(int n = 0; n < 50000; n++) {
		iseg1.classifyIsect(iseg2);
		iseg1.classifyIsect(iseg4);
	}
	time = getTime() - time;
	xmlPrint("Isect time: % ns / ISegment<qint> pair\n", time * 10000);
}

void testVectorAngles() {
	float2 v1(1, 0), v2 = normalize(float2(10, 10));

	assertCloseEnough(radToDeg(angleBetween(v1, v2)), 45.0f);
	assertCloseEnough(radToDeg(angleBetween(v2, v1)), 315.0f);
	assertCloseEnough(angleBetween(v1, v1), 0.0f);

	assertCloseEnough(rotateVector(float2(1, 0), fconstant::pi * 0.5f), float2(0, 1));
	assertCloseEnough(angleToVector(fconstant::pi), float2(-1, 0));

	assertCloseEnough(angleTowards(float2(-4, 4), float2(0, 0), float2(4, 4)), degToRad(90.0f));
	assertCloseEnough(angleTowards(float2(-4, 4), float2(0, 0), float2(-4, 4)), degToRad(180.0f));

	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(-1, 0)), degToRad(135.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(-1, 2)), degToRad(45.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(0, 2)), degToRad(0.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(1, 2)), degToRad(-45.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(1, 0)), degToRad(-135.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(0, 0)), degToRad(-180.0f));
}

static_assert(isVector<short2>(), "");
static_assert(isVector<float4>(), "");
static_assert(!isVector<vector<int>>(), "");

static_assert(isRealObject<FRect>(), "");
static_assert(isIntegralObject<IBox>(), "");
static_assert(isIntegralVector<int3>(), "");

void testHash() {
	vector<vector<Segment3<double>>> data;
	for(int n = 0; n < 100; n++) {
		vector<Segment3<double>> segs;
		for(int x = 0; x < 100; x++)
			segs.emplace_back(double3(randomTranslation(100)), double3(randomTranslation(100)));
		data.emplace_back(segs);
	}

	auto time = getTime();
	for(int n = 0; n < 100; n++)
		int hash_value = hash(data);
	time = getTime() - time;
	double bytes = 100.0 * double(data.size() * data[0].size()) * sizeof(Segment3<double>);
	xmlPrint("Hashing time: % ns / byte\n", time * pow(10.0, 9) / bytes);

	// TODO: better test ?
}

void testMain() {
	FBox box({0, -100, 0}, {1200, 100, 720});
	FBox temp({32, 0, 32}, {64, 0.5f, 64});
	ASSERT(overlaps(box, temp));

	testMatrices();
	testRays();
	testIntersections();
	test2DIntersections();
	testVectorAngles();
	testHash();

	float3 vec(0, 0, 1);
	for(auto &s : vec)
		s += 12.0f;
	ASSERT_EQ(vec, float3(12, 12, 13));
	static_assert(std::is_same<decltype(makeRange(vec)), Range<float>>::value, "");
	ASSERT(!isnan(vec) && !isnan(double3(vec)));

	auto float_len = length(float3(1, 2, 3));
	auto double_len = length((double3)int3(2, 3, 4));
	auto int_dot = dot(int2(10, 20), int2(30, 40));
	ASSERT_EQ(vabs(float2(-10.5f, 13.125f)), float2(10.5f, 13.125f));

	static_assert(std::is_same<decltype(float_len), float>::value, "");
	static_assert(std::is_same<decltype(double_len), double>::value, "");
	static_assert(std::is_same<decltype(int_dot), int>::value, "");

	ASSERT_EQ(xmlFormat("%", double3(1, 2, 3)), xmlFormat("%", float3(1, 2, 3)));

	/*
	Quat rot = normalize(Quat::fromYawPitchRoll(0.5, 1.2, 0.3));
	Quat p = normalize(pow(rot, 1.0f));

	float3 v1 = mulNormal((Matrix4)rot, vec);
	float3 v2 = mulNormal((Matrix4)p, vec);

	printf("result:\n%f %f %f %f\n%f %f %f %f\n", rot.x, rot.y, rot.z, rot.w, p.x, p.y, p.z,
	p.w);
	printf("result:\n%f %f %f\n%f %f %f\n", v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);*/
}
