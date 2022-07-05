// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class SlabAllocator;

// TODO: add support for dedicated allocation
// TODO: use alloc/unmanagedAlloc for frame allocator
// TODO: memory mapping managed here

DEFINE_ENUM(VAllocationType, slab, unmanaged, frame);

class VulkanMemoryManager {
  public:
	static constexpr int num_frames = 2;

	VulkanMemoryManager(VDeviceRef);
	~VulkanMemoryManager();

	VulkanMemoryManager(const VulkanMemoryManager &) = delete;
	void operator=(const VulkanMemoryManager &) = delete;

	void addSlabAllocator(VMemoryDomain, u64 zone_size = 64 * 1024 * 1024);
	Ex<void> allocSlabZone(VMemoryDomain, u64 zone_size);

	struct Budget {
		i64 heap_budget = -1;
		i64 heap_usage = -1;
	};

	bool isAvailable(VMemoryDomain domain) const { return m_domains[domain].type_index != -1; }

	// Will return -1 if budget extension is not available
	EnumMap<VMemoryDomain, Budget> budget() const;

	struct AllocId {
		AllocId(VAllocationType type, VMemoryDomain domain, u32 block_id)
			: type(type), domain(domain), block_id(block_id) {}

		// TODO: encode in u64?
		VAllocationType type;
		VMemoryDomain domain;
		u32 block_id;
	};

	// TODO: allocation should also return size, because it may return more than was requested
	struct Allocation {
		AllocId identifier;
		VkDeviceMemory handle;
		u64 offset;
	};

	Ex<Allocation> alloc(VMemoryDomain, u64 size, uint alignment);
	Ex<Allocation> unmanagedAlloc(VMemoryDomain, u64 size, uint alignment);

	Ex<Allocation> frameAlloc(u64 size, uint alignment);
	Ex<void> frameAlloc(PVBuffer);

	// Note: frame allocations don't have to be freed at all
	void immediateFree(AllocId);
	void deferredFree(AllocId);

  private:
	void beginFrame();
	friend class VulkanDevice;

  private:
	static u64 slabAlloc(u64, uint, void *);

	struct FrameInfo {
		Maybe<AllocId> alloc_id;
		VkDeviceMemory handle = nullptr;
		u64 base_offset = 0, offset = 0;
		u64 size = 0;
	};

	struct DomainInfo {
		VMemoryDomain domain;
		VulkanDevice *device;

		int type_index = -1;
		int heap_index = -1;
		u64 heap_size = 0;
		u64 slab_zone_size = 0;

		vector<PVDeviceMemory> slab_memory;
		vector<PVDeviceMemory> free_memory;
		Dynamic<SlabAllocator> slab_alloc;
	};

	VulkanDevice *m_device = nullptr;
	VkDevice m_device_handle = nullptr;
	EnumMap<VMemoryDomain, DomainInfo> m_domains;

	array<FrameInfo, num_frames> m_frames;
	array<vector<AllocId>, num_frames> m_deferred_frees;

	VMemoryDomain m_frame_allocator_domain;
	u64 m_frame_allocator_base_size = 256 * 1024;
	bool m_is_initial_frame = true;
	int m_frame_index = 0;
};

}