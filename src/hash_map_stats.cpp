// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/hash_map_stats.h"

#include "fwk/algorithm.h"
#include "fwk/format.h"
#include "fwk/vector.h"

namespace fwk {
static constexpr u32 unused_hash = 0xffffffff;
static constexpr u32 deleted_hash = 0xfffffffe;

HashMapStats::HashMapStats() = default;

HashMapStats::HashMapStats(CSpan<u32> hashes) {
	vector<u32> hash_set;
	hash_set.reserve(hashes.size());

	capacity = hashes.size();
	double avg_len = 0.0;

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
		int num_probes = 0;
		int length = 0;

		while(hashes[idx] != unused_hash) {
			num_probes++;
			idx += num_probes;
			while(idx >= capacity)
				idx -= capacity;
			length++;
		}
		avg_len += length;
	}
	avg_hit_sequence_length = avg_len / numUsed();

	makeSortedUnique(hash_set);
	num_distinct_hashes = hash_set.size();
}

void HashMapStats::print() const {
	fwk::print("HashMap stats:\n");
	auto occupied_percent = capacity == 0 ? 0 : double(numOccupied()) * 100.0 / capacity;
	auto distinct_percent =
		numOccupied() == 0 ? 0 : double(num_distinct_hashes) * 100.0 / numOccupied();

	printf("  capacity: %6d occupied: %6d (%.2f%%)\n   deleted: %6d   unused: %6d\n", capacity,
		   numOccupied(), occupied_percent, num_deleted, num_unused);
	printf("            distinct hashes: %d (%.2f%%)\n", num_distinct_hashes, distinct_percent);
	printf("  average hit search length: %.2f\n", avg_hit_sequence_length);
}

void HashMapStats::printOccupancyMap(CSpan<u32> hashes) const {
	fwk::print("[[");
	for(auto &hash : hashes)
		fwk::print("%", hash == unused_hash ? ' ' : hash == deleted_hash ? '.' : 'o');
	fwk::print("]]\n");
}
}
