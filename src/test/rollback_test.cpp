// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/random.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/on_fail.h"
#include "fwk/sys/rollback.h"
#include "fwk/unique_ptr.h"
#include "testing.h"

#include <mutex>
#include <thread>

using namespace fwk;

// This is not acceptable in rollback-enabled context
struct Entity {
	static vector<Entity *> entitys[16];

	Entity(int tid) : tid(tid) {
		index = entitys[tid].size();
		entitys[tid].emplace_back(this);
	}
	~Entity() {
		auto &vec = entitys[tid];
		DASSERT_GT(vec.size(), index);
		vec[index] = vec.back();
		vec[index]->index = index;
		vec.pop_back();
	}

	Entity(const Entity &) = delete;
	void operator=(const Entity &) = delete;

	int index;
	int tid;
};

vector<Entity *> Entity::entitys[16];

vector<int> processingFunction(int tid, int seed, int isize, int osize) {
	vector<int> values;
	Random rand(seed);
	for(int n = 0; n < isize; n++)
		values.emplace_back(rand.uniform(n, n * n));

	vector<vector<int>> others;

	RollbackContext::pause();
	static vector<UniquePtr<Entity>> ents[16];
	ents[tid].clear();
	RollbackContext::resume();

	for(int n = 0; n < osize; n++) {
		rand.permute<int>(values);

		RollbackContext::pause();
		// Without pause this could cause a Segfault:
		ents[tid].emplace_back(uniquePtr<Entity>(tid));
		RollbackContext::resume();

		others.emplace_back(values);
		if(rand.uniform(osize) == 0)
			RollbackContext::rollback(Error());
	}

	return others[rand.uniform(others.size())];
}

void rollbackTest(int tid, int repeats, int inner_size, int outer_size) {
	for(int n = 0; n < repeats; n++) {
		auto result = RollbackContext::begin(
			[&]() { return processingFunction(tid, n, inner_size, outer_size); });

		if(result) {
			//	print("[%] Computation succeeded\n", tid);
		} else {
			//	print("[%] Computation failed\n", tid);
		}
	}
}

template <int size> void testMalloc() {
	int count = 100000;
	vector<void *> ptrs(count);
	double time = getTime();
	for(int n = 0; n < count; n++)
		ptrs[n] = malloc(size);
	time = getTime() - time;
	print("Malloc(%) time: % ns\n", size, time / double(count) * 1000000000.0);
}

void testMallocs() {
	testMalloc<16>();
	testMalloc<64>();
	testMalloc<256>();
	testMalloc<1024>();
	testMalloc<1024 * 32>();
}

void simpleRollbackTest() {
	auto simple_test = []() {
		bool result = !!RollbackContext::begin([]() {
			vector<int> a = {10};
			RollbackContext::rollback(Error());
		});
		ASSERT(!result);
	};

	for(int n = 0; n < 10000; n++) {
		std::thread thread(simple_test);
		thread.join();
	}
}

void multiRollbackTest(int nthreads, int repeats = 20, int inner_size = 1000,
					   int outer_size = 1000) {
	DASSERT(nthreads <= 16);
	vector<std::thread> threads;

	double time = getTime();
	for(int n = 0; n < nthreads; n++)
		threads.emplace_back(rollbackTest, n, repeats, inner_size, outer_size);
	for(auto &thread : threads)
		thread.join();
	time = getTime() - time;
	print("Multi-thread(%) rollback test: % sec\n", nthreads, time);
}

void assertTest2() {
	ASSERT(onFailStackSize() == 1);
	CHECK_FAILED("inner message");
}

void assertTest1() {
	string local_data = "middle message";
	ON_FAIL_FUNC(([](const string &str) { return str; }), local_data);
	assertTest2();
}

void assertTest() {
	auto result = RollbackContext::begin([]() { assertTest1(); });
	ASSERT(!result);
	string err_str = toString(result.error());
	ASSERT(onFailStackSize() == 0);

	ASSERT(err_str.find("middle") != string::npos);
	ASSERT(err_str.find("inner") != string::npos);
}

void testMain() {
	simpleRollbackTest();
	multiRollbackTest(4);
	assertTest();
}
