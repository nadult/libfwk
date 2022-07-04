// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/list_node.h"
#include "fwk/sparse_vector.h"

namespace fwk {

// Memory in this allocator is organized in large (64 - 512MB) zones
// Each zone contains multiple (1 - 32) groups and each group contains 64 slabs.
//
// Those slabs are then divided or merged to allocate chunks. Chunks can have a wide
// range in size, starting from 256 going even up to several MB.
// Single slab can contain many small chunks, but to allocate single large chunk, we
// might need to allocate many slabs.
//
// Supported chunk sizes: 256, 384, 512, 768, 1024, 1536, ..., 2MB, 3MB, 4MB, 6MB, 8MB
// When allocating memory, the size will be rounded up to >= chunk size; This will cause
// small (12.5%) internal fragmentation
//
// Minimum alignment: 128
//
// TODO: option to control chunk alignment
// TODO: functions for defragmentation/garbage collection
class SlabAllocator {
  public:
	static constexpr int min_slabs = 64, max_slabs = 2048;
	static constexpr int slab_group_size = 64, slab_group_shift = 6;

	static constexpr uint chunkSize(int level) {
		uint size0 = 256u << (level >> 1);
		uint size1 = level & 1 ? size0 >> 1 : 0;
		return size0 + size1;
	}

	static constexpr int findBestChunkLevel(u32 size) {
		int level0 = (31 - countLeadingZeros((size - 1) >> 8));
		u32 value0 = 256u << level0;
		u32 value = value0 + (value0 >> 1);
		return (level0 << 1) + (size > value ? 2 : 1);
	}

	static constexpr int num_chunk_levels = 31;
	// Adjust if necessary
	static constexpr int max_chunks_per_group = 1024;
	static constexpr int max_chunk_groups = 128 * 1024;
	static constexpr int max_zones = 64 * 1024;
	static constexpr u64 maxAllocationSize() { return chunkSize(num_chunk_levels - 1); }

	// TODO: consider turning these into template params
	SlabAllocator(u64 slab_size = 256 * 1024, u64 zone_size = 128 * 1024 * 1024);
	~SlabAllocator();

	u64 zoneSize() const { return m_zone_size; }

	struct Chunk {
		Chunk(int chunk_id, int group_id, int level_id)
			: value(u32(chunk_id) | u32((group_id) << 10) | (u32(level_id) << 27)) {
			PASSERT(chunk_level >= 0 && chunk_level < num_chunk_levels);
			PASSERT(chunk_id >= 0 && chunk_id < max_chunks_per_group);
		}

		int chunkId() const { return int(value & 0x3ff); }
		int groupId() const { return int((value >> 10) & 0x1ffff); }
		int levelId() const { return int(value >> 27); }

		u32 value;
	};

	struct Allocation {
		uint zone_id, zone_offset;
	};

	Pair<Chunk, Allocation> alloc(u32 size);
	void free(Chunk);

	// TODO: stats functions

	void testSlabs();
	void visualizeSlabs() const;
	Ex<void> verifySlabs() const;

  private:
	struct Zone {
		int num_free_slabs;
		u32 empty_groups;
		u32 full_groups;
	};

	struct ChunkGroup {
		u16 zone_id, slab_offset;
		int num_free_chunks;
		ListNode node;
	};

	struct ChunkLevel {
		vector<ChunkGroup> groups;
		vector<u64> chunks;

		List not_full_groups;
		uint chunk_size;
		int chunks_per_group;
		int bits_64_per_group;
		int slabs_per_group;
	};

	void addChunkGroup(ChunkLevel &);

	void fillSlabs(int zone_id, int offset, int num_slabs);
	void clearSlabs(int zone_id, int offset, int num_slabs);
	Pair<int, int> allocSlabs(int num_slabs);
	void addZone();

	vector<u64> m_zone_groups;
	vector<Zone> m_zones;
	ChunkLevel m_levels[num_chunk_levels];

	u64 m_slab_size, m_zone_size;
	int m_slabs_per_zone;
	int m_groups_per_zone;
};

}