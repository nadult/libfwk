// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/list_node.h"
#include "fwk/sparse_vector.h"

namespace fwk {

// Linear allocators should be hooked to RenderGraph or Device so that
// memory will be freed at an appropriate time
// TODO: separate allocation logic from vulkan ? Like in BestFit ?
// TODO: better name
class SlabAllocator {
  public:
	static constexpr int min_slabs = 64, max_slabs = 2048;
	static constexpr int group_size = 64, group_shift = 6;

	// Memory in this allocator is organized in large (64 - 512MB) zones
	// Each zone contains multiple (1 - 32) groups and each group contains 64 slabs.
	// Chunk is the smallest allocation unit in a given zone.
	//
	// Those slabs are then divided or merged to allocate chunks. Chunks can have a wide
	// range in size, starting from 256 going even up to several MB.
	// Single slab can contain many small chunks, but to allocate single large chunk, we
	// might need to allocate many slabs.

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

	void fillSlabs(int zone_id, int offset, int num_slabs);
	void clearSlabs(int zone_id, int offset, int num_slabs);

	vector<u64> m_zone_groups;
	vector<Zone> m_zones;

	u64 m_slab_size, m_zone_size;
	int m_slabs_per_zone;
	int m_groups_per_zone;
};

}