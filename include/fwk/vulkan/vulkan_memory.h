// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/slab_allocator.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

// TODO: add support for dedicated allocation

class VulkanAllocator {
  public:
	VulkanAllocator(VDeviceRef, VMemoryDomain);
	~VulkanAllocator();

	using Identifier = SlabAllocator::Identifier;
	struct Allocation {
		Identifier identifier;
		VkDeviceMemory mem_handle;
		u64 mem_offset;
	};

	Ex<Allocation> alloc(u64 size, uint alignment);
	void free(Identifier); // TODO: defer list

  private:
	VulkanDevice *m_device;
	VkDevice m_device_handle;
	uint m_memory_type;
	vector<PVDeviceMemory> m_device_mem;
	SlabAllocator m_slabs;
};

// TODO: use temporary or host domain
// TODO: frame allocator could use basic allocator and allocate slabs directly?
class VulkanFrameAllocator {
  public:
	static constexpr int max_frames = 2;

	VulkanFrameAllocator(VDeviceRef, VMemoryDomain, u64 base_size = 64 * 1024);
	FWK_MOVABLE_CLASS(VulkanFrameAllocator);

	void startFrame(int frame_idx); // TODO: name

	struct Allocation {
		PVDeviceMemory memory;
		u64 offset;
	};
	Ex<Allocation> alloc(u64 size, uint alignment);
	Ex<void> alloc(PVBuffer);

	struct Pool {
		PVDeviceMemory memory;
		u64 offset = 0, size = 0;
	};

  private:
	VulkanDevice *m_device;
	VkDevice m_device_handle;
	uint m_memory_type_index;
	array<Pool, max_frames> m_pools;
	int m_frame_idx = -1;
	u64 m_base_size;
};

}