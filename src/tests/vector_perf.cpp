// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "fwk/indexed_vector.h"
#include "fwk/math/random.h"
#include "fwk/math_base.h"
#include "fwk/sys/memory.h"
#include "fwk/vector.h"
#include "timer.h"

using fwk::TestTimer;

struct Pixel {
	Pixel() {}

	Pixel(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}

	unsigned char r, g, b;
};

template <template <typename> class T> void testVector(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 500; ++i) {
		int dimension = 500;

		T<Pixel> pixels;
		pixels.resize(dimension * dimension);
		for(int i = 0; i < dimension * dimension; ++i) {
			pixels[i].r = 255;
			pixels[i].g = i & 255;
			pixels[i].b = 0;
		}
	}
}

template <template <typename> class T> void testVectorPushBack(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 200; ++i) {
		int dimension = 500;

		T<Pixel> pixels;
		pixels.reserve(dimension * dimension);
		for(int i = 0; i < dimension * dimension; ++i)
			pixels.push_back(Pixel(255, 0, 0));
	}
}

template <template <typename> class T> void testVectorVector(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 100; ++i) {
		T<T<fwk::int3>> temp;
		for(int i = 0; i < 10000; ++i) {
			T<fwk::int3> tout;
			tout.reserve(8);

			for(int axis = 0; axis < 3; axis++) {
				fwk::int3 npos(1, 2, 3);
				npos[axis] += 1;
				tout.emplace_back(npos);
				npos[axis] -= 2;
				tout.emplace_back(npos);
			}
			temp.emplace_back(tout);
		}
	}
}

template <template <typename> class T> void testVectorInsertBack(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 30; ++i) {
		T<fwk::int3> temp;
		for(int i = 0; i < 200; ++i) {
			T<fwk::int3> tout;
			tout.reserve(8);

			for(int axis = 0; axis < 3; axis++) {
				fwk::int3 npos(1, 2, 3);
				npos[axis] += 1;
				tout.emplace_back(npos);
				npos[axis] -= 2;
				tout.emplace_back(npos);
			}
			for(int j = 0; j < 200; j++)
				temp.insert(end(temp), begin(tout), end(tout));
		}
	}
}

template <template <typename> class T> void testVectorInsert(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 100; ++i) {
		T<fwk::int3> temp;
		for(int i = 0; i < 100; ++i) {
			T<fwk::int3> tout;
			tout.reserve(8);
			for(int axis = 0; axis < 3; axis++) {
				fwk::int3 npos(1, 2, 3);
				npos[axis] += 1;
				tout.emplace_back(npos);
				npos[axis] -= 2;
				tout.emplace_back(npos);
			}
			int offset = temp.empty() ? 0 : rand() % temp.size();
			for(int n = 0; n < 10; n++)
				temp.insert(temp.begin() + offset, begin(tout), end(tout));
		}
	}
}

// ------------- IndexedVector tests ----------------------------------------

template <class T> using stdvec = std::vector<T, fwk::SimpleAllocator<T>>;
struct Struct {
	fwk::int4 i4;
	fwk::float2 f2;
};

int iterationLoop(fwk::IndexedVector<Struct> &ivec, int n) NOINLINE;
int iterationLoop(fwk::IndexedVector<Struct> &ivec, int n) {
	int val = 0;
	for(auto i : ivec.indices())
		val += ivec[i].i4[n & 3];
	//for(auto &v : ivec)
	//	val += v.i4[n & 3];
	return val;
}

int testIndexedVector() NOINLINE;
int testIndexedVector() {
	using namespace fwk;
	IndexedVector<Struct> ivec;

	Struct s1{{1, 2, 3, 4}, {2, 3}};
	Struct s2{{1, 2, 5, 4}, {2, 3}};

	{
		Random rand;
		TestTimer t("IndexedVector modification");
		for(int t = 0; t < 1000; t++) {
			for(int n = 0; n < 1000; n++)
				ivec.emplace(s1);
			for(int j = 0; j < 1500; j++) {
				auto idx = rand.uniform(ivec.endIndex());
				if(ivec.valid(idx))
					ivec.erase(idx);
			}
		}
	}

	int val = 0;
	int num_values = 0, num_iters = 0;
	double val_time, total_time;
	{
		auto time = getTime();
		for(int n = 0; n < 1000; n++) {
			val += iterationLoop(ivec, n);
			num_iters += ivec.endIndex();
			num_values += ivec.size();
		}
		total_time = getTime() - time;
		val_time = total_time / num_iters;
	}

	printf("IndexedVector iteration completed in %.4f msec\n", total_time * 1000.0);
	printf("Values: %.2f %%; %f ns / value\n", double(num_values) * 100.0 / double(num_iters),
		   total_time * 1000000000.0 / num_values);
	return val;
}

// ------------- Main function --------------------------------------------------

int main() {
#ifdef FWK_PARANOID
	printf("This test doesn't make sense with FWK_PARANOID enabled\n");
#endif

	testVector<fwk::vector>("fwk::Vector simple");
	testVector<fwk::PoolVector>("fwk::PoolVector simple");
	testVector<stdvec>("std::vector simple");
	printf("\n");

	testVectorPushBack<fwk::vector>("fwk::Vector push_back");
	testVectorPushBack<fwk::PoolVector>("fwk::PoolVector push_back");
	testVectorPushBack<stdvec>("std::vector push_back");
	printf("\n");

	testVectorVector<fwk::vector>("fwk::Vector vector^2");
	testVectorVector<fwk::PoolVector>("fwk::PoolVector vector^2");
	testVectorVector<stdvec>("std::vector vector^2");
	printf("\n");

	testVectorInsertBack<fwk::vector>("fwk::Vector insert_back");
	testVectorInsertBack<fwk::PoolVector>("fwk::PoolVector insert_back");
	testVectorInsertBack<stdvec>("std::vector insert_back");
	printf("\n");

	testVectorInsert<fwk::vector>("fwk::Vector insert");
	testVectorInsert<fwk::PoolVector>("fwk::PoolVector insert");
	testVectorInsert<stdvec>("std::vector insert");
	printf("\n");

	// TODO: special test showing performance of PoolVector

	testIndexedVector();
	return 0;
}
