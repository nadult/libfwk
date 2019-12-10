// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/hash_map.h"
#include "fwk/hash_map_stats.h"
#include "fwk/math/random.h"
#include "fwk/tag_id.h"
#include "testing.h"
#include <unordered_map>

template <class Map>
vector<Pair<string, string>> testMap1(Map &map, const char *name, int num_iters, int &check) {
	constexpr int num_strings = 16;
	const char *strings[] = {"xxx",  "yyy",  "zzz",  "xxx", "abc", "abc",	  "zzz",	"ax",
							 "aaxx", "ddxx", "ccdd", "123", "234", "aaaabbb4", "ssfsdf", "45t98js"};
	Random rand;

	auto time = getTime();

	map.clear();

	int num_found = 0;
	for(int n = 0; n < num_iters; n++) {
		int val = rand.uniform(num_strings * num_strings * 4);
		int id1 = (val / 4) % num_strings;
		int id2 = (val / (4 * num_strings)) % num_strings;

		if(val & 1)
			map[strings[id1]] = strings[id2];
		else if(val & 2)
			num_found += map.find(strings[id1]) != map.end();
		else
			map.erase(strings[id1]);
	}

	time = getTime() - time;

	printf("%20s performance test[%d]: %f ns / iter\n", name, num_iters,
		   time * 1000000000.0 / num_iters);

	vector<Pair<string>> out;
	out.reserve(map.size());
	for(auto &[key, value] : map)
		out.emplace_back(key, value);
	check = num_found;
	return out;
}

using MyTag = TagId<0, u16>;
template <class Map>
vector<Pair<MyTag, string>> testMap2(Map &map, const char *name, int num_iters, int &check) {
	constexpr int num_strings = 16;
	const char *strings[] = {"xxx",  "yyy",  "zzz",  "xxx", "abc", "abc",	  "zzz",	"ax",
							 "aaxx", "ddxx", "ccdd", "123", "234", "aaaabbb4", "ssfsdf", "45t98js"};
	Random rand;
	map.clear();

	auto time = getTime();

	int num_found = 0, num_elems = 1024;
	for(int n = 0; n < num_iters; n++) {
		int val = rand.uniform(num_elems * num_strings * 4);
		MyTag id1((val / 4) % num_elems);
		int id2 = (val / (4 * num_elems)) % num_strings;

		if(val & 1)
			map[id1] = strings[id2];
		else if(val & 2)
			num_found += map.find(id1) != map.end();
		else
			map.erase(id1);
	}

	time = getTime() - time;

	printf("%20s performance test[%d]: %f ns / iter\n", name, num_iters,
		   time * 1000000000.0 / num_iters);

	vector<Pair<MyTag, string>> out;
	out.reserve(map.size());

	for(auto [key, value] : map)
		out.emplace_back(MyTag(key), value);
	check = num_found;
	return out;
}

void microTest() {
	print("HashMap test #1 (Micro const char* -> const char*):\n");
	int check1, check2;

	HashMap<const char *, const char *> map_fwk;
	std::unordered_map<const char *, const char *> map_std;

	auto result_fwk = testMap1(map_fwk, "fwk::HashMap", 2048, check1);
	auto result_std = testMap1(map_std, "std::unordered_map", 2048, check2);
	HashMapStats(map_fwk).print(true);

	makeSorted(result_fwk);
	makeSorted(result_std);
	ASSERT_EQ(result_fwk, result_std);
	ASSERT_EQ(check1, check2);
}

void miniTest() {
	struct Policy {
		using Storage = HashMapStorageSeparated<MyTag, const char *>;
		static u32 hash(MyTag tag) { return u32(tag) ^ (u32(tag) << 10); }
	};

	HashMap<MyTag, const char *, Policy> map_fwk;
	std::unordered_map<u16, const char *> map_std;

	print("HashMap test #2 (Small TagId<u16> -> const char*)\n");
	int check1, check2;
	auto result_fwk = testMap2(map_fwk, "fwk::HashMap", 2048, check1);
	auto result_std = testMap2(map_std, "std::unordered_map", 2048, check2);
	HashMapStats(map_fwk).print(true);

	testMap2(map_fwk, "fwk::HashMap", 2048 * 32, check1);
	testMap2(map_std, "std::unordered_map", 2048 * 32, check2);
	HashMapStats(map_fwk).print(true);

	makeSorted(result_fwk);
	makeSorted(result_std);
	ASSERT_EQ(result_fwk, result_std);
	ASSERT_EQ(check1, check2);
}

void testMain() {
	microTest();
	miniTest();
}
