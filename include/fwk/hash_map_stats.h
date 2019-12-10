// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/span.h"

namespace fwk {

struct HashMapStats {
	template <class K, class V, class P>
	HashMapStats(const HashMap<K, V, P> &hash_map) : HashMapStats(hash_map.hashes()) {
		used_memory = hash_map.usedMemory();
	}
	template <class K> HashMapStats(const HashSet<K> &hash_set) : HashMapStats(hash_set.hashes()) {
		used_memory = hash_set.usedMemory();
	}

	HashMapStats(CSpan<u32> hashes);
	HashMapStats();

	void print(bool print_occupancy_map) const;

	int capacity = 0;
	i64 used_memory = 0;
	int num_unused = 0, num_deleted = 0;
	float avg_hit_sequence_length = 0.0f;
	int max_hit_sequence_length = 0;
	int num_distinct_hashes = 0;
	string occupancy_map;

	int numUsed() const { return capacity - num_unused; }
	int numOccupied() const { return capacity - num_unused - num_deleted; }
};
}
