// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/pod_vector.h"
#include "fwk/vulkan_base.h"

#include <vulkan/vulkan.h>

namespace fwk {

struct VulkanObjectId {
	VulkanObjectId() : bits(0) {}
	VulkanObjectId(VDeviceId device_id, int object_id)
		: bits(u32(object_id) | (u32(device_id) << 28)) {
		PASSERT(object_id >= 0 && object_id <= 0xfffffff);
		PASSERT(uint(device_id) < 16);
	}
	explicit VulkanObjectId(u32 bits) : bits(bits) {}

	bool valid() const { return bits != 0; }
	explicit operator bool() const { return bits != 0; }

	int objectId() const { return int(bits & 0xfffffff); }
	VDeviceId deviceId() const { return VDeviceId(bits >> 28); }

	bool operator==(const VulkanObjectId &rhs) const { return bits == rhs.bits; }
	bool operator<(const VulkanObjectId &rhs) const { return bits < rhs.bits; }

	u32 bits;
};

// Manages lifetimes of most of Vulkan objects.
// Objects are destroyed when ref-count drops to 0 and
// several (2-3 typically) release phases passed.
// Release phase should change when a frame has finished rendering.
// Idea from: https://www.gamedev.net/forums/topic/677665-safe-resource-deallocation-in-vulkan/5285533/
//
// It has to be initialized before use.
struct VulkanObjectManager {
	VulkanObjectId add(VDeviceId device_id, void *handle);

	void acquire(VulkanObjectId);
	void release(VulkanObjectId);

	void assignRef(VulkanObjectId lhs, VulkanObjectId rhs);

	void initialize(VTypeId, int reserve = 16);
	// TODO: clear ?
	void nextReleasePhase(VDeviceId, VkDevice);

	static constexpr uint empty_node = 0;

	// Keeps ref-counts (if used) or free-list nodes (if unused)
	// Index 0 is always unused, never ends up in a free list
	vector<u32> counters;
	vector<void *> handles;
	// TODO: use vector, max_vulkan_devices won't be needed
	Array<u32, 3> to_be_released_lists[max_vulkan_devices];
	u32 free_list;
	VTypeId type_id;
};
}