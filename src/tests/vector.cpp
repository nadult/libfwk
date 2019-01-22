// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "testing.h"

void testMain() {
	vector<int> vec = {10, 20, 40, 50};

	vector<vector<int>> vvals;
	for(int k = 0; k < 4; k++)
		vvals.emplace_back(vec);
	vvals.erase(vvals.begin() + 1);
	vvals.erase(vvals.begin() + 2);
	ASSERT(toString(vvals) == "10 20 40 50 10 20 40 50");

	vec.insert(vec.begin() + 2, 30);
	vec.erase(vec.begin() + 3, vec.end());
	auto copy = vec;
	ASSERT(toString(copy) == "10 20 30");

	vector<string> vecs = {"xxx", "yyy", "zzz", "xxx", "abc", "abc", "zzz"};
	ASSERT(toString(sortedUnique(vecs)) == "abc xxx yyy zzz");
}
