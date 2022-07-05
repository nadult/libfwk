// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/slab_allocator.h"

#include "fwk/format.h"
#include "fwk/index_range.h"
#include "fwk/math/random.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"

namespace fwk {

static int findClosestChunk(u32 size) {
	int level0 = (31 - countLeadingZeros((size - 1) >> 8));
	u32 value0 = 256u << level0;
	u32 value = value0 + (value0 >> 1);
	return (level0 << 1) + (size > value ? 2 : 1);
}

static u64 defaultZoneAllocator(u64 size, uint, void *) { return size; }

SlabAllocator::SlabAllocator(u64 default_zone_size)
	: SlabAllocator(default_zone_size, {defaultZoneAllocator}) {}
SlabAllocator::SlabAllocator(u64 default_zone_size, ZoneAllocator zone_alloc)
	: m_zone_allocator(zone_alloc), m_default_zone_size(default_zone_size) {
	DASSERT(validZoneSize(default_zone_size));

	for(int l : intRange(num_chunk_levels)) {
		auto &level = m_levels[l];
		level.chunk_size = chunkSize(l);

		level.slabs_per_group = max<int>(1, (level.chunk_size + slab_size - 1) / slab_size);
		double waste = 1.0;
		while(true) {
			u64 group_size = u64(level.slabs_per_group) * slab_size;
			level.chunks_per_group = group_size / level.chunk_size;
			waste = 1.0 - double(level.chunks_per_group * level.chunk_size) / group_size;
			if(waste < 0.05)
				break;
			level.slabs_per_group++;
		}

		level.bits_64_per_group = (level.chunks_per_group + 63) >> 6;
		DASSERT_LE(level.chunks_per_group, max_chunks_per_group);
		/*printf("SlabAllocator level %3d: chunk_size:%u group_size:%lluK chunks/group:%d "
			   "slabs/group:%d waste:%f\n",
			   l, level.chunk_size, slab_size * level.slabs_per_group / 1024,
			   level.chunks_per_group, level.slabs_per_group, waste);*/
	}

	for(int l = 0; l < num_chunk_levels; l++)
		DASSERT_EQ(findBestChunkLevel(chunkSize(l)), l);
}

SlabAllocator::~SlabAllocator() = default;

#define GROUP_ACCESSOR [&](int idx) -> ListNode & { return level.groups[idx].node; }

// Here we assume that bits are not empty
constexpr int findLastBit(u64 bits) { return __builtin_clzll(bits); }
constexpr int findFirstBit(u64 bits) { return __builtin_ctzll(bits); }

auto SlabAllocator::alloc(u64 size) -> Pair<Identifier, Allocation> {
	if(size > maxChunkSize()) {
		int num_slabs = (size + slab_size - 1) / slab_size;
		auto [zone_id, slab_id] = allocSlabs(num_slabs);
		if(zone_id == -1)
			return {};

		u64 zone_offset = u64(slab_id) * slab_size;
		return {Identifier(slab_id, num_slabs, zone_id), Allocation{uint(zone_id), zone_offset}};
	}

	int level_id = findBestChunkLevel(size);
	auto &level = m_levels[level_id];

	if(level.not_full_groups.empty()) {
		auto [zone_id, slab_id] = allocSlabs(level.slabs_per_group);
		if(zone_id == -1)
			return {};
		int group_index = level.groups.size();
		level.groups.emplace_back(ChunkGroup{.num_free_chunks = level.chunks_per_group,
											 .slab_offset = u16(slab_id),
											 .zone_id = u16(zone_id)});
		level.chunks.resize(level.groups.size() * level.bits_64_per_group, 0ull);
		listInsert(GROUP_ACCESSOR, level.not_full_groups, group_index);
	}

	int group_id = level.not_full_groups.head;
	auto &group = level.groups[group_id];
	if(--group.num_free_chunks == 0) {
		level.not_full_groups.head = group.node.next;
		if(level.not_full_groups.tail == group_id)
			level.not_full_groups.tail = -1;
		group.node = {};
	}

	int chunk_id = -1;
	u64 *chunk_bits = &level.chunks[group_id * level.bits_64_per_group];
	for(int i = 0; i < level.bits_64_per_group; i++)
		if(chunk_bits[i] != ~0ull) {
			int bit = findFirstBit(~chunk_bits[i]);
			chunk_bits[i] |= 1ull << bit;
			chunk_id = (i << 6) + bit;
		}
	DASSERT(chunk_id != 1 && chunk_id < level.chunks_per_group);

	Identifier ident(chunk_id, group_id, level_id, 0);
	u64 zone_offset = u64(group.slab_offset * slab_size) + u64(chunk_id) * level.chunk_size;
	Allocation alloc{group.zone_id, zone_offset};
	return {ident, alloc};
}

void SlabAllocator::free(Identifier ident) {
	if(ident.isChunkAlloc()) {
		int level_id = ident.chunkLevelId(), group_id = ident.chunkGroupId();
		int chunk_id = ident.chunkId();
		DASSERT_LT(level_id, num_chunk_levels);

		auto &level = m_levels[level_id];
		auto &group = level.groups[group_id];

		if(++group.num_free_chunks == 1)
			listInsert(GROUP_ACCESSOR, level.not_full_groups, group_id);
		uint bits_idx = group_id * level.bits_64_per_group + (chunk_id >> 6);
		level.chunks[bits_idx] |= 1ull << (chunk_id & 63);
	} else {
		int slab_id = ident.slabId(), zone_id = ident.slabZoneId();
		int num_slabs = ident.slabCount();
		clearSlabs(zone_id, slab_id, num_slabs);
	}

	// TODO: free unused groups
}

#undef GROUP_ACCESSOR

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
	// TODO: we can make it quite fast with SSE/AVX probably
	// The number of groups per zone should be small in typical case (4 - 16)

	int target_zone = -1, target_offset = -1;
	for(int i : intRange(m_zones)) {
		auto &zone = m_zones[i];
		if(zone.num_free_slabs < num_slabs)
			continue;

		int groups_per_zone = zone.groups.size();
		uint not_full_groups = (~zone.full_groups) & ((1 << groups_per_zone) - 1);
		u64 *groups = zone.groups.data();
		if(num_slabs == 1) {
			int first_group = findFirstBit(not_full_groups);
			int first_bit = findFirstBit(~groups[first_group]);
			target_offset = (first_group << slab_group_shift) + first_bit;
		} else if(num_slabs <= 64) {
			int first_group = findFirstBit(not_full_groups);
			for(int g = first_group; g < zone.num_slab_groups; g++) {
				if(groups[g] == ~0ull)
					continue;
				int offset = findNBits(~groups[g], num_slabs);
				if(offset == -1 && g + 1 < zone.num_slab_groups) {
					int cur_space = countLeadingZeros(groups[g]);
					int next_space = countTrailingZeros(groups[g + 1]);
					if(cur_space + next_space >= num_slabs)
						offset = 64 - cur_space;
				}
				if(offset != -1) {
					target_offset = (g << slab_group_shift) + offset;
					break;
				}
			}
		} else {
			int first_group = findFirstBit(not_full_groups);
			for(int g = first_group; g < zone.num_slab_groups; g++) {
				if(groups[g] == ~0ull)
					continue;
				int cur_space = countLeadingZeros(groups[g]);
				int total_space = cur_space;

				for(int j = g + 1; j < zone.num_slab_groups; j++) {
					int space = countTrailingZeros(groups[j]);
					total_space += space;
					if(space < 64 || total_space >= num_slabs)
						break;
				}
				if(total_space >= num_slabs) {
					target_offset = (g << slab_group_shift) + 64 - cur_space;
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

		u64 min_size =
			((num_slabs * slab_size + min_zone_size - 1) / min_zone_size) * min_zone_size;
		if(!allocZone(max(m_default_zone_size, min_size)))
			return {-1, -1};
	}

	fillSlabs(target_zone, target_offset, num_slabs);
	return {target_zone, target_offset};
}

bool SlabAllocator::allocZone(u64 zone_size) {
	DASSERT(validZoneSize(zone_size));
	DASSERT_LE(m_zones.size(), max_zones);

	zone_size = m_zone_allocator.func(zone_size, m_zones.size(), m_zone_allocator.param);
	if(zone_size == 0)
		return false;
	DASSERT(validZoneSize(zone_size));

	Zone &zone = m_zones.emplace_back();
	zone.num_slabs = int(zone_size / slab_size);
	zone.num_free_slabs = zone.num_slabs;
	zone.num_slab_groups = zone.num_slabs / slab_group_size;
	zone.groups_mask = (1ull << zone.num_slab_groups) - 1;
	zone.empty_groups = zone.groups_mask;
	zone.full_groups = 0;
	zone.groups.resize(zone.num_slab_groups, 0);
	return true;
}

void SlabAllocator::fillSlabs(int zone_id, int offset, int num_slabs) {
	auto &zone = m_zones[zone_id];
	DASSERT(offset + num_slabs <= zone.num_slabs);

	int first_group = offset >> slab_group_shift;
	int last_group = (offset + num_slabs - 1) >> slab_group_shift;
	u64 *groups = zone.groups.data();

	int first_offset = offset & (slab_group_size - 1);
	u64 first_bits = (~0ull >> max(0, 64 - num_slabs)) << first_offset;
	groups[first_group] |= first_bits;

	if(last_group != first_group) {
		int num_last_bits = (offset + num_slabs) & (slab_group_size - 1);
		u64 last_bits = ~0ull >> (64 - num_last_bits);
		groups[last_group] |= last_bits;
	}

	for(int g = first_group + 1; g < last_group; g++)
		groups[g] = ~0ull;

	int num_groups = last_group - first_group + 1;
	int num_full_groups = max(num_groups - 2, 0);

	zone.num_free_slabs -= num_slabs;
	zone.empty_groups &=
		~((zone.groups_mask >> (zone.num_slab_groups - num_groups)) << first_group);
	uint full_bits =
		((zone.groups_mask >> (zone.num_slab_groups - num_full_groups)) << (first_group + 1));
	if(groups[first_group] == ~0ull)
		full_bits |= 1u << first_group;
	if(groups[last_group] == ~0ull)
		full_bits |= 1u << last_group;
	zone.full_groups |= full_bits;
}

void SlabAllocator::clearSlabs(int zone_id, int offset, int num_slabs) {
	auto &zone = m_zones[zone_id];
	DASSERT(offset + num_slabs <= zone.num_slabs);

	int first_group = offset >> slab_group_shift;
	int last_group = (offset + num_slabs - 1) >> slab_group_shift;
	u64 *groups = zone.groups.data();

	int first_offset = offset & (slab_group_size - 1);
	u64 first_bits = (~0ull >> max(0, 64 - num_slabs)) << first_offset;
	groups[first_group] &= ~first_bits;

	if(last_group != first_group) {
		int num_last_bits = (offset + num_slabs) & (slab_group_size - 1);
		u64 last_bits = ~0ull >> (64 - num_last_bits);
		groups[last_group] &= ~last_bits;
	}

	for(int g = first_group + 1; g < last_group; g++)
		groups[g] = 0;

	int num_groups = last_group - first_group + 1;
	int num_full_groups = max(num_groups - 2, 0);

	zone.num_free_slabs += num_slabs;
	zone.full_groups &= ~((zone.groups_mask >> (zone.num_slab_groups - num_groups)) << first_group);
	uint empty_bits =
		((zone.groups_mask >> (zone.num_slab_groups - num_full_groups)) << (first_group + 1));
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
		print("Zone %: num_slabs:% num_free:% empty_groups:% full_groups:%\n", s, zone.num_slabs,
			  zone.num_free_slabs, formatBits(zone.empty_groups, zone.groups.size()),
			  formatBits(zone.full_groups, zone.groups.size()));
		for(int g : intRange(zone.groups))
			printf("Group %3d: %s\n", g, formatBits(zone.groups[g]).c_str());
		print("\n");
	}
}

Ex<void> SlabAllocator::verifySlabs() const {
	for(int s : intRange(m_zones)) {
		auto &zone = m_zones[s];
		int num_slabs = 0;
		for(int g : intRange(zone.groups)) {
			bool is_empty = zone.groups[g] == 0;
			bool is_full = zone.groups[g] == ~0ull;
			num_slabs += countBits(zone.groups[g]);

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
		int num_free_slabs = zone.num_slabs - num_slabs;
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
		DASSERT(zone_id != -1);
		allocs.emplace_back(zone_id, slab_offset, num_slabs);
		print("Allocating: % slabs\n", num_slabs);
		visualizeSlabs();
		verifySlabs().check();
	}

	for(int n = 0; n < 128; n++) {
		int mode = rand.uniform(0, 2);
		if(mode || allocs.empty()) {
			int num_slabs = max<int>(1, rand.uniform(1, 128));
			auto [zone_id, slab_offset] = allocSlabs(num_slabs);
			DASSERT(zone_id != -1);
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