// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/hash_map_stats.h"

#include "fwk/algorithm.h"
#include "fwk/format.h"
#include "fwk/vector.h"

namespace fwk {

HashMapStats::HashMapStats() = default;

HashMapStats::HashMapStats(CSpan<u32> hashes) {
	constexpr u32 unused_hash = ~0u, deleted_hash = ~1u;

	vector<u32> hash_set;
	hash_set.reserve(hashes.size());

	capacity = hashes.size();
	uint capacity_mask = capacity - 1;
	vector<int> lengths(hashes.size(), 0);

	for(int n = 0; n < hashes.size(); n++) {
		if(hashes[n] == unused_hash) {
			num_unused++;
			continue;
		}
		if(hashes[n] != deleted_hash)
			hash_set.emplace_back(hashes[n]);
		else
			num_deleted++;

		int idx = n;
		int num_probes = 1;
		int length = 1;

		while(hashes[idx] != unused_hash) {
			idx = (idx + num_probes++) & capacity_mask;
			length++;
		}

		lengths[n] = length;
		max_hit_sequence_length = max(max_hit_sequence_length, length);
	}

	double avg_len = 0.0;
	for(auto hash : hashes)
		if(hash < deleted_hash) {
			int start_idx = hash & capacity_mask;
			avg_len += lengths[start_idx];
		}
	avg_hit_sequence_length = avg_len / numUsed();

	occupancy_map.resize(hashes.size(), ' ');
	for(int n = 0; n < hashes.size(); n++)
		if(hashes[n] == deleted_hash)
			occupancy_map[n] = '.';
		else if(hashes[n] < deleted_hash)
			occupancy_map[n] = 'o';

	makeSortedUnique(hash_set);
	num_distinct_hashes = hash_set.size();
}

void HashMapStats::print(bool print_occ_map) const {
	fwk::print("HashMap stats:\n");
	auto occupied_percent = capacity == 0 ? 0 : double(numOccupied()) * 100.0 / capacity;
	auto distinct_percent =
		numOccupied() == 0 ? 0 : double(num_distinct_hashes) * 100.0 / numOccupied();

	printf("  capacity: %6d occupied: %6d (%.2f%%)\n   deleted: %6d   unused: %6d\n", capacity,
		   numOccupied(), occupied_percent, num_deleted, num_unused);
	printf("            distinct hashes: %d (%.2f%%)\n", num_distinct_hashes, distinct_percent);
	printf("  average hit search length: %.2f\n", avg_hit_sequence_length);
	if(used_memory)
		printf("                used memory: %lld KB\n", used_memory / 1024);

	if(print_occ_map)
		fwk::print("  occupancy map: [[%]]\n", occupancy_map);
}
}
