// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/affine_trans.h"
#include "fwk/math/axis_angle.h"
#include "fwk/math/box_iter.h"
#include "fwk/math/cylinder.h"
#include "fwk/math/ext24.h"
#include "fwk/math/gcd.h"
#include "fwk/math/hash.h"
#include "fwk/math/matrix4.h"
#include "fwk/math/obox.h"
#include "fwk/math/random.h"
#include "fwk/math/rational.h"
#include "fwk/math/ray.h"
#include "fwk/math/rotation.h"
#include "fwk/math/segment.h"
#include "fwk/math/tetrahedron.h"
#include "fwk/math/triangle.h"
#include "fwk/sys/assert.h"
#include "fwk/variant.h"
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
		frand() * pi * 2.0f)));
}

AffineTrans randomTransform() {
	return AffineTrans(randomTranslation(50.0f), randomRotation(), randomScale());
}

void testMatrices() {
	float3 up(0, 1, 0);

	for(int n = 0; n < 100; n++) {
		float3 trans = randomTranslation(100.0f);
		float3 scale = randomScale();
		Quat rot = randomRotation();

		Matrix4 mat = translation(trans) * Matrix4(rot) * scaling(scale);
		AffineTrans dec(mat);
		assertCloseEnough(trans, dec.translation, 0.00001);
		assertCloseEnough(scale, dec.scale, 0.00001);
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
	Triangle3F tri1(float3(0, 0, 4), float3(0, 2, 4), float3(2, 0, 4));
	Triangle3F tri2(float3(1, 0, 1), float3(6, 0, 1), float3(1, 0, 6));

	Segment3<float> segment1(0.5, 0.5, 0, 0.5, 0.5, 10);
	Segment3<float> segment2(1.3, 1.3, 0, 1.0, 1.0, 10);

	assertCloseEnough(segment1.isectParam(tri1).first.closest(), 0.4f);
	ASSERT(!segment2.isectParam(tri1).first);
	assertCloseEnough(tri1.surfaceArea(), 2.0f);

	auto angles2 = tri2.angles();
	assertCloseEnough(float3(angles2[0], angles2[1], angles2[2]), float3(0.5f, 0.25f, 0.25f) * pi);

	Segment3<float> segment3(float3(1, 1, 0), float3(4, 4, 0));
	float3 p1(4, 1, 0), p2(0.5, 0.5, 0), p3(5, 4, 0);
	assertCloseEnough(segment3.closestPoint(p1), float3(2.5, 2.5, 0));
	assertCloseEnough(segment3.closestPoint(p2), float3(1, 1, 0));
	assertCloseEnough(segment3.closestPoint(p3), float3(4, 4, 0));

	auto ray = *segment3.asRay();
	assertCloseEnough(ray.closestPoint(p1), float3(2.5, 2.5, 0), 0.00001);
	assertCloseEnough(ray.closestPoint(p2), float3(0.5, 0.5, 0), 0.00001);
	assertCloseEnough(ray.closestPoint(p3), float3(4.5, 4.5, 0), 0.00001);

	Segment3<float> segment4({3, 2, 0}, {6, 5, 0});
	Segment3<float> segment5({6, 7, 0}, {8, 5, 0});
	auto ray4 = *segment4.asRay(), ray5 = *segment5.asRay();
	assertCloseEnough(segment3.distance(segment4), std::sqrt(2.0f) / 2.0f);
	assertCloseEnough(segment4.distance(segment5), std::sqrt(2.0f));
	assertCloseEnough(ray4.distance(ray5), 0.0f);
}

void testIntersections() {
	ASSERT_EQ(distance(Cylinder(float3(1, 2, 3), 0.5, 2.0), float3(2, 2, 3)), 0.5f);
	ASSERT_EQ(distance(Cylinder(float3(1, 1, 1), 1.5, 2.0), float3(2, 1, 1)), 0.0f);
	ASSERT_EQ(distance(Cylinder(float3(2, 2, 2), 1.5, 2.0), float3(2, 5, 2)), 1.0f);

	Triangle3F tri(float3(0, 0, 0), float3(1, 0, 0), float3(0, 1, 0));
	Segment3<float> seg(1, 1, -1, 1, 1, 1);

	ASSERT(!seg.isectParam(tri).first);
	ASSERT_EQ(tri.distance(float3(1, 1, 0)), std::sqrt(2.0f) / 2.0f);

	/*
	Triangle3F tri1(float3(4.330130, 10.000000, 2.500000), float3(-4.330130, 10.000000, 2.500000),
				  float3(0.000000, 1.730360, 0.000000));
	Triangle3F tri2(float3(-2.500000, 0.000000, -4.330130), float3(4.330130, 10.000000, 2.500000),
				  float3(-2.500000, 0.000000, 4.330130));
	ASSERT(areIntersecting(tri1, tri2));*/

	Tetrahedron tet({0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0.25, 1, 0.25});
	ASSERT_EQ(tet.volume(), 1.0f / 6.0f);
}

void testBox() {
	FBox bbox1({0, 0, 0}, {1, 1, 1});
	FBox bbox2({0.49, 0, 0.49}, {1, 1, 1});
	FBox bbox3({0.45, 0.5, 0.45}, {2, 2, 2});

	//	ASSERT_EQ(areIntersecting(tet, bbox1), true);
	//	ASSERT_EQ(areIntersecting(tet, bbox2), true);
	//	ASSERT_EQ(areIntersecting(tet, bbox3), false);

	for(int n = 0; n < 1000; n++) {
		float3 points[4];
		for(int i = 0; i < 4; i++)
			points[i] = float3(int3(randomTranslation(100.0f)));

		FBox box1 = enclose(span({points[0], points[1]}));
		FBox box2 = enclose(span({points[2], points[3]}));

		// TODO: fix sat
		//ASSERT_EQ(areOverlapping(box1, box2), satTest(box1, box2));
	}

	FBox box({0, -100, 0}, {1200, 100, 720});
	FBox temp({32, 0, 32}, {64, 0.5f, 64});
	ASSERT(overlaps(box, temp));
}

void testOBox() {
	OBox<int2> box1({-1, 0}, {2, 3}, {1, -2});
	OBox<int2> box2({3, 2}, {3, 7}, {9, 2});
	OBox<int2> box3({2, 1}, {5, 2}, {1, 4});
	OBox<int2> box4({8, 9}, {9, 10}, {11, 6});
	OBox<int2> box5({5, 1}, {7, 1}, {5, 8});

	ASSERT(box1.isIntersecting(box3));
	ASSERT(box2.isIntersecting(box3));
	ASSERT(box2.isIntersecting(box5));

	ASSERT(!box2.isIntersecting(box4));
	ASSERT(!box2.isIntersecting(box1)); // touches
}

template <class T> void print(const typename Segment<T, 2>::Isect &isect) {
	if(const Segment2<T> *seg = isect)
		print("Segment(% - %)\n", seg->from, seg->to);
	else if(const vec2<T> *vec = isect)
		print("Vector %\n", *vec);
	else
		print("Empty\n");
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

	auto r4 = s4.asRay();
	auto r6 = s6.asRay();
	ASSERT(r4 && r6);
	ASSERT(!(Segment2<double>{-1, -1, -1, -1}).asRay());
	auto isect_param = r4->isectParam(*r6);
	ASSERT(isect_param.isPoint() && r4->at(isect_param.asPoint()) == double2(2.5, 2.5));

	ASSERT_EQ(s6.closestPointParam({0.5, 2.5}), 0.5);

	Segment2<double> seg1(-5.6, -9.1, -4.2, -9.5);
	Segment2<double> seg2(-4.1, -9.4, -2.4, -9.2);
	ASSERT(seg1.isect(seg2) == none);

	using ISeg = Segment2<int>;
	using IClass = IsectClass;

	ISeg iseg1(0, 0, 943782983, 999999999), iseg2(0, 1, 1000000123, 2);
	ISeg iseg3(-1, 0, 943782982, 999999999), iseg4(-123456789, 934567893, 985473892, -848372819);
	ASSERT(iseg1.classifyIsect(iseg2) == IClass::point);
	ASSERT(iseg1.classifyIsect(iseg3) == IClass::none);
	ASSERT(iseg1.classifyIsect(iseg4) == IClass::point);

	ASSERT_EQ(ISeg(0, 0, 10, 0).classifyIsect(ISeg(0, 0, 5, 0)), IClass::segment);
	ASSERT_EQ(ISeg(0, 0, 10, 0).classifyIsect(ISeg(10, 0, 11, 0)), IClass::adjacent);
	ASSERT_EQ(ISeg(0, 0, 10, 0).classifyIsect(ISeg(-1, 0, 0, 0)), IClass::adjacent);
	ASSERT_EQ(ISeg(0, 0, 10, 0).classifyIsect(ISeg(0, 10, 0, 0)), IClass::adjacent);
	ASSERT_EQ(ISeg(0, 0, 2, 0).classifyIsect(int2(1, 0)), IClass::point);
	ASSERT_EQ(ISeg(0, 0, 5, 5).classifyIsect(int2(3, 3)), IClass::point);
	ASSERT_EQ(ISeg(0, 0, 5, 5).classifyIsect(int2(5, 5)), IClass::adjacent);
	ASSERT_EQ(ISeg(0, 0, 5, 5).classifyIsect(int2(2, 3)), IClass::none);

	ASSERT(!ISeg(-1, 0, 10, 2).testIsect(IRect(1, 1, 4, 4)));
	ASSERT(ISeg(-3, 0, 10, 2).testIsect(IRect(1, 1, 4, 4)));

	ISeg seg5(1, 1, 4, 4);
	ASSERT(seg5.classifyIsect(ISeg(3, 3, 3, 3)) == IClass::point);

	auto time = getTime();
	for(int n = 0; n < 50000; n++) {
		s3.isect(s4);
		s6.isect(s4);
	}
	time = getTime() - time;
	print("Isect time: % ns / Segment<double> pair\n", time * 10000);

	time = getTime();
	for(int n = 0; n < 50000; n++) {
		iseg1.classifyIsect(iseg2);
		iseg1.classifyIsect(iseg4);
	}
	time = getTime() - time;
	print("Isect time: % ns / ISegment<int> pair\n", time * 10000);

	Triangle2F tri(float2(0, 0), float2(5, 0), float2(2, 4));
	using f2 = Pair<float>;
	ASSERT_EQ(tri.barycentric(float2(2, 4)), f2(0, 1));
	ASSERT_EQ(tri.barycentric(float2(3.5, 2)), f2(0.5, 0.5));
	ASSERT_EQ(tri.barycentric(float2(2, 0)), f2(0.4, 0));

	ISeg seg6(1, 1, 3, 4), seg7(1, 4, 4, 3);
	auto t = seg6.isectParam(seg7).asPoint();
	auto pt = Rational2<llint>(seg6.from) + Rational2<llint>(seg6.dir()) * t;
	ASSERT_EQ(pt, Rational2<llint>({29, 38}, 11));
}

void test3DIntersections() {
	Triangle3D tri1(double3(-1, 0, -1), double3(1, 0, -1), double3(1, 0, 1));
	Box3<double> box1(double3(-1, -1, -1), double3(1, 1, 1));
	ASSERT(tri1.testIsect(box1));

	Triangle3D tri0(double3(0, 0, 3), double3(4, 0, 1), double3(5, 0, 4));
	Box3<double> box0(double3(0, -0.001, 0), double3(3, 1, 2));
	ASSERT(tri0.testIsect(box0));
}

void testVectorAngles() {
	float2 v1(1, 0), v2 = normalize(float2(10, 10));

	assertCloseEnough(radToDeg(angleBetween(v1, v2)), 45.0f);
	assertCloseEnough(radToDeg(angleBetween(v2, v1)), 315.0f);
	assertCloseEnough(angleBetween(v1, v1), 0.0f);

	assertCloseEnough(rotateVector(float2(1, 0), pi * 0.5f), float2(0, 1));
	assertCloseEnough(angleToVector((float)pi), float2(-1, 0));

	assertCloseEnough(angleTowards(float2(-4, 4), float2(0, 0), float2(4, 4)), degToRad(90.0f));
	assertCloseEnough(angleTowards(float2(-4, 4), float2(0, 0), float2(-4, 4)), degToRad(180.0f));

	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(-1, 0)), degToRad(135.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(-1, 2)), degToRad(45.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(0, 2)), degToRad(0.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(1, 2)), degToRad(-45.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(1, 0)), degToRad(-135.0f));
	assertCloseEnough(angleTowards(float2(0, 0), float2(0, 1), float2(0, 0)), degToRad(-180.0f));

	ASSERT(sameDirection(int2(2, 3), int2(4, 6)));
	ASSERT(sameDirection(int3(-2, 5, 17), int3(-6, 15, 51)));

	Random rand;
	for(int n : intRange(1000)) {
		auto vec = rand.sampleBox(float3(-1000), float3(1000));
		vec = normalize(vec);
		ASSERT_HINT(isNormalized(vec), vec);
	}
}

static_assert(is_vec<short2>);
static_assert(is_vec<float4>);
static_assert(!is_vec<vector<int>>);

static_assert(is_same<Scalar<FRect>, float>);
static_assert(is_integral<Base<IBox>>);

void testHash() {
	vector<vector<Segment3<double>>> data;
	for(int n = 0; n < 100; n++) {
		vector<Segment3<double>> segs;
		for(int x = 0; x < 100; x++)
			segs.emplace_back(double3(randomTranslation(100)), double3(randomTranslation(100)));
		data.emplace_back(segs);
	}

	ASSERT_EQ(hashMany<u64>(1.0f, 2.0f, 3.0f),
			  combineHash<u64>(hash<u64>(1.0f), hash<u64>(2.0f), hash<u64>(3.0f)));

	struct Dummy {
		int hash() const { return 123; }
	};
	ASSERT_EQ(hash(Dummy()), (uint)Dummy().hash());

	enum class eval { a, b, c };
	auto ehash = hash(eval::a);

	auto time = getTime();
	for(int n = 0; n < 100; n++)
		int hash_value = hash(data);
	time = getTime() - time;
	double bytes = 100.0 * double(data.size() * data[0].size()) * sizeof(Segment3<double>);
	print("Hashing time: % ns / byte\n", time * pow(10.0, 9) / bytes);

	// TODO: better test ?
}

void testTraits() {
	static_assert(precise_conversion<qint, qint>);
	static_assert(precise_conversion<int2, llint2>);
	static_assert(!precise_conversion<llint2, double2>);
	static_assert(is_same<Promote<llint>, qint>);
	static_assert(is_same<Promote<float>, double>);

	static_assert(is_same<PromoteIntegral<float>, float>);
	static_assert(is_same<PromoteIntegral<int2>, llint2>);

	static_assert(is_same<Promote<Rational<int>, 2>, Rational<qint>>);
	static_assert(is_same<Promote<Ext24<short>, 2>, Ext24<llint>>);
	static_assert(is_same<Promote<RatExt24<short>>, RatExt24<int>>);
}

template <class T> int approxSign(const Ext24<T> &ext) {
	using LD = long double;
	return LD(ext.b) * sqrt2 + LD(ext.c) * sqrt3 + LD(ext.d) * sqrt6 + LD(ext.a) < 0 ? -1 : 1;
}

void testExt24() {
	Random rand;
	for(int n = 0; n < 100000; n++) {
		int b = rand.uniform(-500000000, 500000000);
		int c = rand.uniform(-500000000, 500000000);
		int d = rand.uniform(-500000000, 500000000);
		int a = -int(b * sqrtl(2) + c * sqrtl(3) + d * sqrtl(6));
		if(allOf(cspan({a, b, c, d}), 0))
			continue;

		Ext24 ext(a, b, c, d);
		int sign = ext.sign();
		if(sign != approxSign(ext)) {
			print("ERROR: % + % * sq2 + % * sq3 + % * sq6 = % (sign: %)\n", a, b, c, d,
				  a + b * sqrt(2) + c * sqrt(3) + d * sqrt(6), sign);
			//quadSignPrint(a, b, c, d);
			print("\n\n");
		}
	}

	{
		int a = 128;
		int b = 23;
		int c = 99;
		double time = getTime();
		int iters = 1000000;
		int sum = 0;
		for(int n : intRange(iters)) {
			int d = n - iters / 16;
			sum += Ext24(a, b, c, d).sign();
		}
		print("Quad24::compare: % ns [%]\n", (getTime() - time) * 1000000000.0 / double(iters),
			  sum);
	}

	using Ex = Ext24<int>;
	using ExVec = MakeVec<Ext24<int>, 2>;
	Segment2<Ex> seg1{ExVec{ext_sqrt3<int>, 1}, ExVec{0, -2}};
	Segment2<Ex> seg2{ExVec{-Ex(ext_sqrt3<int>), -1}, ExVec{ext_sqrt3<int>, -1}};

	int iters = 100000;
	RatExt24<llint> sum = 0;

	double time = getTime();
	for(int n = 0; n < iters; n++) {
		auto result = seg1.isectParam(seg2);
		auto pt = result.asPoint();
		sum += result.asPoint().den();
	}
	time = getTime() - time;
	print("Isect time: % ns / Segment2<Ext24<int>> pair\n", time * 1000000000.0 / double(iters));
	ASSERT_EQ(sum, ext_sqrt3<int> * (iters * 6));

	ASSERT_EQ((ext_sqrt3<int> * 2 + 1 + ext_sqrt6<int> * 6) *
				  (ext_sqrt2<int> * 10 - ext_sqrt3<int> * 4),
			  Ext24<int>(-24, -62, 116, 20));

	auto isect_pos = seg1.isectParam(seg2).closest();
	auto isect_normalized = isect_pos.num() * RatExt24<llint>(isect_pos.den().intDenomInverse());
	ASSERT_EQ(isect_normalized, RatExt24<llint>(2, 3));

	ASSERT_EQ(Ex(1, 5, 0, 0).intDenomInverse(), ratDivide(Ex(-1, 5, 0, 0), 49));
	ASSERT_EQ(Ex(1, 0, 0, 1).intDenomInverse(), ratDivide(Ex(-1, 0, 0, 1), 5));
	ASSERT_EQ(Ex(100, 100, 101, 0).intDenomInverse(),
			  ratDivide(Ex(4060300, 2060300, -60903, -2020000), 799636391));

	{ // Testing rotations
		Rat2Ext24<int> vec({1, 0}, 1), vec_sum;
		auto prev = rotateVector(vec, -15);

		for(int n = 0; n < 24; n++) {
			auto rvec = rotateVector(vec, n * 15);
			auto angle_diff = angleBetween(double2(prev), double2(rvec));
			assertCloseEnough(angle_diff, degToRad(15.0));
			vec_sum += rvec;
			prev = rvec;
		}
		ASSERT_EQ(vec_sum, Rat2Ext24<int>());
	}
}

void testRational() {
	ASSERT_GT(Rational<int>(1, 0), Rational<int>(100, 1));
	ASSERT_LT(Rational<int>(-1, 0), Rational<int>(-1000, 2));
	ASSERT_LT(Rational<int>(-1, 0), Rational<int>(1, 0));
	ASSERT_NE(Rational<int>(1, 0), Rational<int>(-1, 0));

	ASSERT_EQ(Rational2<int>({10, 20}, 10), Rational2<int>({5, 10}, 5));
	ASSERT_EQ(double2(Rational2<int>({1, 2}, 10)), double2(0.1, 0.2));
	ASSERT_EQ(rationalApprox(sqrt(3.0), 10, 10), Rational<int>(7, 4));

	Random rand;
	for(int n = 0; n < 100000; n++) {
		Rational<int> a(rand.uniform(-1000000, 1000000), rand.uniform(1, 1000000));
		Rational<int> b(rand.uniform(-1000000, 1000000), rand.uniform(1, 1000000));
		DASSERT((a < b) == (double(a) < double(b)));
	}

	int iters = 100000;
	long long max = 1000000000000000000ll;

	vector<Pair<qint>> qnumbers;
	vector<Pair<llint>> lnumbers;
	for(int n : intRange(iters)) {
		qint v1 = qint(rand.uniform(0ll, max)) * rand.uniform(1ll, max);
		qint v2 = qint(rand.uniform(0ll, max)) * rand.uniform(1ll, max);
		lnumbers.emplace_back(rand.uniform(0ll, max), rand.uniform(1ll, max));
		qnumbers.emplace_back(v1, v2);
	}

	print("GCD timings:\n");
	{
		double time1 = getTime();
		int sum1 = 0;
		for(auto &pair : lnumbers)
			sum1 += gcdEuclid(pair.first, pair.second);
		time1 = getTime() - time1;

		double time2 = getTime();
		int sum2 = 0;
		for(auto &pair : lnumbers)
			sum2 += gcd(pair.first, pair.second);
		time2 = getTime() - time2;

		print(" 64bit euclideanGCD: % ns (%)\n", time1 * 1000000000.0 / double(iters), sum1);
		print(" 64bit binaryGCD:    % ns (%)\n", time2 * 1000000000.0 / double(iters), sum2);
	}

	{
		double time1 = getTime();
		int sum1 = 0;
		for(auto &pair : qnumbers)
			sum1 += gcdEuclid(pair.first, pair.second);
		time1 = getTime() - time1;

		double time2 = getTime();
		int sum2 = 0;
		for(auto &pair : qnumbers)
			sum2 += gcd(pair.first, pair.second);
		time2 = getTime() - time2;

		print("128bit EuclideanGCD: % ns (%)\n", time1 * 1000000000.0 / double(iters), sum1);
		print("128bit binaryGCD:    % ns (%)\n", time2 * 1000000000.0 / double(iters), sum2);
	}

	{
		double time = getTime();
		int sum = 0;
		for(auto &pair : qnumbers) {
			sum += pair.first / pair.second;
			sum += pair.second / pair.first;
		}
		time = getTime() - time;
		print("QInt div time: % ns (%)\n", time * 1000000000.0 / double(iters * 2), sum);
	}

	{
		double time = getTime();
		int sum = 0;
		for(auto &pair : qnumbers) {
			sum += pair.first * pair.second;
		}
		time = getTime() - time;
		print("QInt mul time: % ns (%)\n", time * 1000000000.0 / double(iters), sum);
	}

	Triangle3<int> tri(int3(0, 0, 0), int3(1000, 0, 0), int3(0, 0, 1000));
	Segment3<int> seg{int3(200, 1000, 200), int3(200, -1000, 200)};

	auto result = seg.isectParam(tri);
	ASSERT_EQ(result.first.closest(), Rational<qint>(1, 2));
}

void testConsts() {
	ASSERT_EQ(toString(double(-inf)), "-inf");
	assertCloseEnough(double(sqrt2) * sqrt2, 2.0);
	// TODO: full long double support
	//assertCloseEnough((long double)sqrt6 * sqrt6, 6.0L);
}

void testMain() {
	Backtrace::t_default_mode = BacktraceMode::full;

	testConsts();
	testRational();
	testMatrices();
	testRays();
	testIntersections();
	test2DIntersections();
	test3DIntersections();
	testVectorAngles();
	testHash();
	testTraits();
	testBox();
	testOBox();
	testExt24();

	float3 vec(0, 0, 1);
	for(auto &s : vec.values())
		s += 12.0f;
	ASSERT_EQ(vec, float3(12, 12, 13));
	static_assert(is_same<decltype(vec.values()), Span<float, 3>>, "");
	ASSERT(!isnan(vec) && !isnan(double3(vec)));

	auto float_len = length(float3(1, 2, 3));
	auto double_len = length((double3)int3(2, 3, 4));
	auto int_dot = dot(int2(10, 20), int2(30, 40));
	ASSERT_EQ(vabs(float2(-10.5f, 13.125f)), float2(10.5f, 13.125f));

	IRect rect(0, 0, 2, 3);
	vector<int2> points;
	for(auto pt : cells(rect))
		points.emplace_back(pt);
	ASSERT_EQ(points, vector<int2>({{0, 0}, {1, 0}, {0, 1}, {1, 1}, {0, 2}, {1, 2}}));

	static_assert(is_same<decltype(float_len), float>, "");
	static_assert(is_same<decltype(double_len), double>, "");
	static_assert(is_same<decltype(int_dot), int>, "");

	ASSERT_EQ(format("%", double3(1, 2, 3)), format("%", float3(1, 2, 3)));

	ASSERT(ccwSide(int2{0, 0}, {2, 0}, {0, 1}));
	int2 vectors[6] = {{2, 3}, {-2, 3}, {-3, 0}, {-4, -2}, {0, -2}, {3, -2}};
	for(auto [i, j] : pairsRange(6))
		ASSERT(ccwSide(vectors[i], vectors[j]));

	/*
	Quat rot = normalize(Quat::fromYawPitchRoll(0.5, 1.2, 0.3));
	Quat p = normalize(pow(rot, 1.0f));

	float3 v1 = mulNormal((Matrix4)rot, vec);
	float3 v2 = mulNormal((Matrix4)p, vec);

	printf("result:\n%f %f %f %f\n%f %f %f %f\n", rot.x, rot.y, rot.z, rot.w, p.x, p.y, p.z,
	p.w);
	printf("result:\n%f %f %f\n%f %f %f\n", v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);*/
}
