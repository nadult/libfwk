// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/slab_allocator.h"

#include "fwk/format.h"
#include "fwk/index_range.h"
#include "fwk/math/random.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"

namespace fwk {

SlabAllocator::SlabAllocator(u64 slab_size, u64 zone_size)
	: m_slab_size(slab_size), m_zone_size(zone_size) {
	DASSERT(zone_size % slab_size == 0);
	u64 num_slabs = zone_size / slab_size;
	DASSERT_GE(num_slabs, min_slabs);
	DASSERT_LE(num_slabs, max_slabs);
	DASSERT(num_slabs % group_size == 0);
	m_slabs_per_zone = num_slabs;
	m_groups_per_zone = num_slabs / group_size;
}
SlabAllocator::~SlabAllocator() = default;

constexpr int findLastBit(u64 bits) { return __builtin_clzll(bits); }
constexpr int findFirstBit(u64 bits) { return __builtin_ctzll(bits); }

// Returns index of first bit from a string of at least N bits or -1
int findNBits(u64 bits, int n) {
	int s;
	while(n > 1) {
		s = n >> 1;
		bits = bits & (bits >> s);
		n -= s;
	}
	return bits ? findFirstBit(bits) : -1;
}

Pair<int, int> SlabAllocator::allocSlabs(int num_slabs) {
	DASSERT(num_slabs >= 1 && num_slabs < m_slabs_per_zone);
	// TODO: we can make it quite fast with SSE/AVX probably
	// The number of groups per zone should be small (4 - 16)

	int target_zone = -1, target_offset = -1;
	for(int i : intRange(m_zones)) {
		auto &zone = m_zones[i];
		if(zone.num_free_slabs < num_slabs)
			continue;

		uint not_full_groups = (~zone.full_groups) & ((1 << m_groups_per_zone) - 1);
		u64 *groups = &m_zone_groups[i * m_groups_per_zone];
		if(num_slabs == 1) {
			int first_group = findFirstBit(not_full_groups);
			int first_bit = findFirstBit(~groups[first_group]);
			target_offset = (first_group << group_shift) + first_bit;
		} else if(num_slabs <= 64) {
			int first_group = findFirstBit(not_full_groups);
			for(int g = first_group; g < m_groups_per_zone; g++) {
				if(groups[g] == ~0ull)
					continue;
				int offset = findNBits(~groups[g], num_slabs);
				if(offset == -1 && g + 1 < m_groups_per_zone) {
					int cur_space = countLeadingZeros(groups[g]);
					int next_space = countTrailingZeros(groups[g + 1]);
					if(cur_space + next_space >= num_slabs)
						offset = 64 - cur_space;
				}
				if(offset != -1) {
					target_offset = (g << group_shift) + offset;
					break;
				}
			}
		} else {
			int first_group = findFirstBit(not_full_groups);
			for(int g = first_group; g < m_groups_per_zone; g++) {
				if(groups[g] == ~0ull)
					continue;
				int cur_space = countLeadingZeros(groups[g]);
				int total_space = cur_space;

				for(int j = g + 1; j < m_groups_per_zone; j++) {
					int space = countTrailingZeros(groups[j]);
					total_space += space;
					if(space < 64 || total_space >= num_slabs)
						break;
				}
				if(total_space >= num_slabs) {
					target_offset = (g << group_shift) + 64 - cur_space;
					break;
				}
			}
		}

		if(target_offset != -1) {
			target_zone = i;
			break;
		}
	}

	if(target_offset == -1) {
		target_offset = 0;
		target_zone = m_zones.size();
		addZone();
	}

	fillSlabs(target_zone, target_offset, num_slabs);
	return {target_zone, target_offset};
}

void SlabAllocator::addZone() {
	int zone_offset = m_zone_groups.size() * m_groups_per_zone;
	Zone new_zone;
	new_zone.num_free_slabs = m_slabs_per_zone;
	new_zone.empty_groups = (1ull << m_groups_per_zone) - 1;
	new_zone.full_groups = 0;
	m_zones.emplace_back(new_zone);
	m_zone_groups.resize(m_zones.size() * m_groups_per_zone, 0);
}

void SlabAllocator::fillSlabs(int zone_id, int offset, int num_slabs) {
	DASSERT(offset + num_slabs <= m_slabs_per_zone);

	int first_group = offset >> group_shift;
	int last_group = (offset + num_slabs - 1) >> group_shift;
	u64 *groups = &m_zone_groups[zone_id * m_groups_per_zone];

	int first_offset = offset & (group_size - 1);
	u64 first_bits = (~0ull >> max(0, 64 - num_slabs)) << first_offset;
	groups[first_group] |= first_bits;

	if(last_group != first_group) {
		int num_last_bits = (offset + num_slabs) & (group_size - 1);
		u64 last_bits = ~0ull >> (64 - num_last_bits);
		groups[last_group] |= last_bits;
	}

	for(int g = first_group + 1; g < last_group; g++)
		groups[g] = ~0ull;

	int num_groups = last_group - first_group + 1;
	int num_full_groups = max(num_groups - 2, 0);

	auto &zone = m_zones[zone_id];
	zone.num_free_slabs -= num_slabs;
	u32 group_bits = (1 << m_groups_per_zone) - 1;
	zone.empty_groups &= ~((group_bits >> (m_groups_per_zone - num_groups)) << first_group);
	uint full_bits = ((group_bits >> (m_groups_per_zone - num_full_groups)) << (first_group + 1));
	if(groups[first_group] == ~0ull)
		full_bits |= 1u << first_group;
	if(groups[last_group] == ~0ull)
		full_bits |= 1u << last_group;
	zone.full_groups |= full_bits;
}

void SlabAllocator::clearSlabs(int zone_id, int offset, int num_slabs) {
	DASSERT(offset + num_slabs <= m_slabs_per_zone);

	int first_group = offset >> group_shift;
	int last_group = (offset + num_slabs - 1) >> group_shift;
	u64 *groups = &m_zone_groups[zone_id * m_groups_per_zone];

	int first_offset = offset & (group_size - 1);
	u64 first_bits = (~0ull >> max(0, 64 - num_slabs)) << first_offset;
	groups[first_group] &= ~first_bits;

	if(last_group != first_group) {
		int num_last_bits = (offset + num_slabs) & (group_size - 1);
		u64 last_bits = ~0ull >> (64 - num_last_bits);
		groups[last_group] &= ~last_bits;
	}

	for(int g = first_group + 1; g < last_group; g++)
		groups[g] = 0;

	int num_groups = last_group - first_group + 1;
	int num_full_groups = max(num_groups - 2, 0);

	auto &zone = m_zones[zone_id];
	zone.num_free_slabs += num_slabs;
	u32 group_bits = (1 << m_groups_per_zone) - 1;

	zone.full_groups &= ~((group_bits >> (m_groups_per_zone - num_groups)) << first_group);
	uint empty_bits = ((group_bits >> (m_groups_per_zone - num_full_groups)) << (first_group + 1));
	if(groups[first_group] == 0)
		empty_bits |= 1u << first_group;
	if(groups[last_group] == 0)
		empty_bits |= 1u << last_group;
	zone.empty_groups |= empty_bits;
}

template <c_integral Unsigned>
string formatBits(Unsigned bits, int num_bits = sizeof(Unsigned) * 8, int group_size = 8,
				  char delimiter = ' ') {
	TextFormatter fmt;
	for(int i = 0; i < num_bits; i++) {
		if(delimiter && i % group_size == 0 && i != 0)
			fmt << delimiter;
		fmt << (bits & (1ull << i) ? '1' : '0');
	}
	return fmt.text();
}

void SlabAllocator::visualizeSlabs() const {
	for(int s : intRange(m_zones)) {
		auto &zone = m_zones[s];
		print("Zone %: num_free:% empty_groups:% full_groups:%\n", s, zone.num_free_slabs,
			  formatBits(zone.empty_groups, m_groups_per_zone),
			  formatBits(zone.full_groups, m_groups_per_zone));

		auto *groups = &m_zone_groups[s * m_groups_per_zone];
		for(int g : intRange(m_groups_per_zone))
			printf("Group %3d: %s\n", g, formatBits(groups[g]).c_str());
		print("\n");
	}
}

Ex<void> SlabAllocator::verifySlabs() const {
	for(int s : intRange(m_zones)) {
		auto &zone = m_zones[s];
		const u64 *groups = &m_zone_groups[m_groups_per_zone * s];
		int num_slabs = 0;
		for(int g = 0; g < m_groups_per_zone; g++) {
			bool is_empty = groups[g] == 0;
			bool is_full = groups[g] == ~0ull;
			num_slabs += countBits(groups[g]);

			bool marked_empty = zone.empty_groups & (1u << g);
			bool marked_full = zone.full_groups & (1u << g);

			if(is_empty != marked_empty)
				return FWK_ERROR("% group marked as % (zone_id:% group_id:%)",
								 is_empty ? "Empty" : "Not-empty",
								 marked_empty ? "empty" : "not-empty", s, g);

			if(is_full != marked_full)
				return FWK_ERROR("% group marked as % (zone_id:% group_id:%)",
								 is_full ? "Full" : "Not-full", marked_full ? "full" : "not-full",
								 s, g);
		}
		int num_free_slabs = m_slabs_per_zone - num_slabs;
		if(num_free_slabs != zone.num_free_slabs)
			return FWK_ERROR("Invalid number of free slabs: % (should be: %; zone_id:%)",
							 zone.num_free_slabs, num_free_slabs, s);
	}

	return {};
}

void SlabAllocator::testSlabs() {
	struct Allocation {
		int zone_id, slab_offset, num_slabs;
	};
	vector<Allocation> allocs;
	Random rand;

	for(int n = 0; n < 30; n++) {
		int num_slabs = max<int>(1, sqrt(rand.uniform(1, 200)));
		auto [zone_id, slab_offset] = allocSlabs(num_slabs);
		allocs.emplace_back(zone_id, slab_offset, num_slabs);
		print("Allocating: % slabs\n", num_slabs);
		visualizeSlabs();
		verifySlabs().check();
	}

	for(int n = 0; n < 1000; n++) {
		int mode = rand.uniform(0, 2);
		if(mode || allocs.empty()) {
			int num_slabs = max<int>(1, rand.uniform(1, 128));
			auto [zone_id, slab_offset] = allocSlabs(num_slabs);
			allocs.emplace_back(zone_id, slab_offset, num_slabs);
			print("Allocating: % slabs\n", num_slabs);
		} else {
			int alloc_id = rand.uniform(0, allocs.size() - 1);
			auto alloc = allocs[alloc_id];
			clearSlabs(alloc.zone_id, alloc.slab_offset, alloc.num_slabs);
			allocs[alloc_id] = allocs.back();
			allocs.pop_back();
			print("Freeing: zone_id:% slab_offset:% num_slabs:%\n", alloc.zone_id,
				  alloc.slab_offset, alloc.num_slabs);
		}
		visualizeSlabs();
		verifySlabs().check();
	}
}
}