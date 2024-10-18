// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "fwk/math/random.h"
#include "fwk/math_base.h"
#include "fwk/sparse_vector.h"
#include "fwk/sys/memory.h"
#include "fwk/vector.h"
#include "timer.h"

using fwk::TestTimer;

struct Pixel {
	Pixel() {}

	Pixel(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}

	unsigned char r, g, b;
};

template <class T> struct PoolInitializedVec : public fwk::vector<T> {
	PoolInitializedVec() : fwk::vector<T>(fwk::pool_alloc) {}
};

template <template <typename> class Vec> void testVector(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 500; ++i) {
		int dimension = 500;

		Vec<Pixel> pixels;
		pixels.resize(dimension * dimension);
		for(int i = 0; i < dimension * dimension; ++i) {
			pixels[i].r = 255;
			pixels[i].g = i & 255;
			pixels[i].b = 0;
		}
	}
}

template <template <typename> class Vec> void testVectorPushBack(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 200; ++i) {
		int dimension = 500;

		Vec<Pixel> pixels;
		pixels.reserve(dimension * dimension);
		for(int i = 0; i < dimension * dimension; ++i)
			pixels.push_back(Pixel(255, 0, 0));
	}
}

template <template <typename> class Vec> void testVectorVector(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 100; ++i) {
		Vec<Vec<fwk::int3>> temp;

		auto fill = [&](Vec<fwk::int3> &out) {};

		for(int i = 0; i < 10000; ++i) {
			Vec<fwk::int3> tout;
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

template <template <typename> class Vec> void testVectorInsertBack(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 30; ++i) {
		Vec<fwk::int3> temp;
		for(int i = 0; i < 200; ++i) {
			Vec<fwk::int3> tout;
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

template <template <typename> class Vec> void testVectorInsert(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 100; ++i) {
		Vec<fwk::int3> temp;
		for(int i = 0; i < 100; ++i) {
			Vec<fwk::int3> tout;
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

// ------------- SparseVector tests ----------------------------------------

template <class T> using stdvec = std::vector<T, fwk::SimpleAllocator<T>>;
struct Struct {
	fwk::int4 i4;
	fwk::float2 f2;
};

FWK_NO_INLINE int iterationLoop(fwk::SparseVector<Struct> &ivec, int n) {
	int val = 0;
	for(auto i : ivec.indices())
		val += ivec[i].i4[n & 3];
	//for(auto &v : ivec)
	//	val += v.i4[n & 3];
	return val;
}

FWK_NO_INLINE int testSparseVector() {
	using namespace fwk;
	SparseVector<Struct> ivec;

	Struct s1{{1, 2, 3, 4}, {2, 3}};
	Struct s2{{1, 2, 5, 4}, {2, 3}};

	{
		Random rand;
		TestTimer t("SparseVector modification");
		for(int t = 0; t < 1000; t++) {
			for(int n = 0; n < 1000; n++)
				ivec.emplace(s1);
			for(int j = 0; j < 1500; j++) {
				auto idx = rand.uniform(ivec.spread());
				if(ivec.valid(idx))
					ivec.erase(idx);
			}
		}
	}

	int val = 0;
	int num_values = 0, num_iters = 0;
	double val_time [[maybe_unused]], total_time;
	{
		auto time = getTime();
		for(int n = 0; n < 1000; n++) {
			val += iterationLoop(ivec, n);
			num_iters += ivec.spread();
			num_values += ivec.size();
		}
		total_time = getTime() - time;
		val_time = total_time / num_iters;
	}

	printf("SparseVector iteration completed in %.4f msec\n", total_time * 1000.0);
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
	testVector<stdvec>("std::vector simple");
	printf("\n");

	testVectorPushBack<fwk::vector>("fwk::Vector push_back");
	testVectorPushBack<stdvec>("std::vector push_back");
	printf("\n");

	testVectorVector<fwk::vector>("fwk::Vector vector^2");
	testVectorVector<PoolInitializedVec>("fwk::Vector(Pooled) vector^2");
	testVectorVector<stdvec>("std::vector vector^2");
	printf("\n");

	testVectorInsertBack<fwk::vector>("fwk::Vector insert_back");
	testVectorInsertBack<PoolInitializedVec>("fwk::Vector(Pooled) insert_back");
	testVectorInsertBack<stdvec>("std::vector insert_back");
	printf("\n");

	testVectorInsert<fwk::vector>("fwk::Vector insert");
	testVectorInsert<stdvec>("std::vector insert");
	printf("\n");

	// TODO: special test showing performance of PoolVector

	testSparseVector();
	return 0;
}
