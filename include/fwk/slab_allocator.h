// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

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
class SlabAllocator {
  public:
	static constexpr int min_slabs = 64, max_slabs = 2048;
	static constexpr int slab_group_size = 64, slab_group_shift = 6;

	static constexpr uint chunkSize(int level) {
		uint size0 = 256u << (level >> 1);
		uint size1 = level & 1 ? size0 >> 1 : 0;
		return size0 + size1;
	}

	// TODO: better name
	static constexpr int num_levels = 31;
	static constexpr u64 maxAllocationSize() { return chunkSize(num_levels); }

	// TODO: consider turning these into template params
	SlabAllocator(u64 slab_size = 256 * 1024, u64 zone_size = 128 * 1024 * 1024);
	~SlabAllocator();

	int slabsPerZone() const { return m_slabs_per_zone; }
	u64 slabSize() const { return m_slab_size; }
	u64 zoneSize() const { return m_zone_size; }

	Pair<int, int> allocSlabs(int num_slabs);
	void addZone();

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
		int zone_id;
		int slab_offset;
	};

	struct Level {
		uint chunk_size;
		int chunks_per_group;
		int slabs_per_group;
	};

	void fillSlabs(int zone_id, int offset, int num_slabs);
	void clearSlabs(int zone_id, int offset, int num_slabs);

	vector<u64> m_zone_groups;
	vector<Zone> m_zones;
	Level m_levels[num_levels];

	u64 m_slab_size, m_zone_size;
	int m_slabs_per_zone;
	int m_groups_per_zone;
};

}