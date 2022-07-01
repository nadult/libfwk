// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanFrameAllocator {
  public:
	static constexpr int max_memory_types = 32;
	static constexpr int max_frames = 2;

	VulkanFrameAllocator(VDeviceRef, u64 base_size = 64 * 1024);
	FWK_MOVABLE_CLASS(VulkanFrameAllocator);

	void startFrame(int frame_idx); // TODO: name
	Ex<void> alloc(PVBuffer, VMemoryFlags);

	struct Pool {
		PVDeviceMemory memory;
		VMemoryFlags flags = none;
		u64 offset = 0, size = 0;
	};

  private:
	VulkanDevice *m_device;
	VkDevice m_device_handle;
	array<vector<Pool>, max_frames> m_pools;
	int m_frame_idx = -1;
	u64 m_base_size;
};
}