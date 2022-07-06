// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class SlabAllocator;


class VulkanMemoryManager {
  public:
	static constexpr int num_frames = 2;
	static constexpr u32 max_allocation_size = 0xffffffffu;

	VulkanMemoryManager(VkDevice, const VulkanPhysicalDeviceInfo &, VDeviceFeatures);
	~VulkanMemoryManager();

	VulkanMemoryManager(const VulkanMemoryManager &) = delete;
	void operator=(const VulkanMemoryManager &) = delete;

	void addSlabAllocator(VMemoryDomain, u32 zone_size = 64 * 1024 * 1024);
	Ex<void> allocSlabZone(VMemoryDomain, u32 zone_size);

	struct Budget {
		i64 heap_budget = -1;
		i64 heap_usage = -1;
	};

	bool isAvailable(VMemoryDomain domain) const { return m_domains[domain].type_index != -1; }

	// Will return -1 if budget extension is not available
	EnumMap<VMemoryDomain, Budget> budget() const;

	Ex<VMemoryBlock> alloc(VMemoryDomain, u32 size, uint alignment);
	Ex<VMemoryBlock> unmanagedAlloc(VMemoryDomain, u32 size);
	Ex<VMemoryBlock> frameAlloc(u32 size, uint alignment);
	Ex<VMemoryBlock> alloc(VMemoryUsage, const VkMemoryRequirements &);

	// Note: frame allocations don't have to be freed at all
	void immediateFree(VMemoryBlockId);
	void deferredFree(VMemoryBlockId);

	// Memory will be automatically flushed in finishFrame()
	// Memory should be filled immediately after calling accessMemory
	// accessMemory may invalidate previous mappings
	Ex<Span<char>> accessMemory(const VMemoryBlock &);

  private:
	void beginFrame();
	void finishFrame();
	friend class VulkanDevice;

  private:
	void shrunkMappings();
	static u64 slabAlloc(u64, uint, void *);

	struct DeviceMemory {
		VkDeviceMemory handle = nullptr;
		void *mapping = nullptr;
	};

	struct FrameInfo {
		Maybe<VMemoryBlockId> alloc_id;
		DeviceMemory memory;
		u64 base_offset = 0, offset = 0;
		u64 size = 0;
	};

	struct DomainInfo {
		VMemoryDomain domain;
		VkDevice device_handle;

		int type_index = -1;
		int heap_index = -1;
		u64 heap_size = 0;
		u64 slab_zone_size = 0;

		bool validDomain(u32 type_mask) const;

		vector<DeviceMemory> slab_memory;
		vector<DeviceMemory> unmanaged_memory;
		Dynamic<SlabAllocator> slab_alloc;
	};

	VkDevice m_device_handle = nullptr;
	VkPhysicalDevice m_phys_handle = nullptr;
	EnumMap<VMemoryDomain, DomainInfo> m_domains;
	u32 m_non_coherent_atom_size = 1;

	array<FrameInfo, num_frames> m_frames;
	array<vector<VMemoryBlockId>, num_frames> m_deferred_frees;
	vector<VkMappedMemoryRange> m_flush_ranges;

	VMemoryDomain m_frame_allocator_domain;
	u32 m_frame_allocator_base_size = 256 * 1024;
	int m_frame_index = 0;

	bool m_is_initial_frame = true;
	bool m_has_mem_budget = false;
};
}