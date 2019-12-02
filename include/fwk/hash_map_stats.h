// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/span.h"

namespace fwk {

struct HashMapStats {
	template <class K, class V>
	HashMapStats(const HashMap<K, V> &hash_map) : HashMapStats(hash_map.hashes()) {}
	template <class K> HashMapStats(const HashSet<K> &hash_set) : HashMapStats(hash_set.hashes()) {}

	HashMapStats(CSpan<u32> hashes);
	HashMapStats();

	void print() const;
	void printOccupancyMap(CSpan<u32>) const;

	int capacity = 0;
	int num_unused = 0, num_deleted = 0;
	float avg_hit_sequence_length = 0.0f;
	int num_distinct_hashes = 0;

	int numUsed() const { return capacity - num_unused; }
	int numOccupied() const { return capacity - num_unused - num_deleted; }
};

}
