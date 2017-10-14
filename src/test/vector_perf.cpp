// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "fwk_math.h"
#include "timer.h"

using fwk::TestTimer;

struct Pixel {
	Pixel() {}

	Pixel(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}

	unsigned char r, g, b;
};

template <template <typename> class T> void testVector(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 1000; ++i) {
		int dimension = 500;

		T<Pixel> pixels;
		pixels.resize(dimension * dimension);
		for(int i = 0; i < dimension * dimension; ++i) {
			pixels[i].r = 255;
			pixels[i].g = 0;
			pixels[i].b = 0;
		}
	}
}

template <template <typename> class T> void testVectorPushBack(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 1000; ++i) {
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

	for(int i = 0; i < 100; ++i) {
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

	for(int i = 0; i < 500; ++i) {
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

template <class T> using stdvec = std::vector<T, fwk::SimpleAllocator<T>>;

int main() {
	testVector<fwk::vector>("fwk::vector simple");
	testVector<stdvec>("std::vector simple");
	testVectorPushBack<fwk::vector>("fwk::vector push_back");
	testVectorPushBack<stdvec>("std::vector push_back");
	testVectorVector<fwk::vector>("fwk::vector vector^2");
	testVectorVector<stdvec>("std::vector vector^2");
	testVectorInsertBack<fwk::vector>("fwk::vector insert_back");
	testVectorInsertBack<stdvec>("std::vector insert_back");
	testVectorInsert<fwk::vector>("fwk::vector insert");
	testVectorInsert<stdvec>("std::vector insert");
	return 0;
}
