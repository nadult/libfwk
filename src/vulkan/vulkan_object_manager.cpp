// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_object_manager.h"
#include "fwk/vulkan/vulkan_ptr.h"

#include "fwk/format.h"
#include "fwk/gfx/opengl.h"
#include "fwk/hash_map.h"
#include "fwk/sys/assert.h"
#include <vulkan/vulkan.h>

namespace fwk {

void VulkanObjectManager::initialize(VTypeId type_id, int reserve) {
	this->type_id = type_id;
	counters.reserve(reserve);
	handles.reserve(reserve);
	counters.clear();
	handles.clear();
	counters.emplace_back(0u);
	handles.emplace_back(nullptr);
	for(auto &tbr_list : to_be_released_lists)
		fill(tbr_list, empty_node);
	free_list = empty_node;
}

VulkanObjectId VulkanObjectManager::add(VDeviceId device_id, void *handle) {
	PASSERT(handle);

	int object_id = 0;
	if(free_list != empty_node) {
		object_id = int(free_list);
		free_list = counters[free_list];
		counters[object_id] = 1; // TODO: 0 ?
		handles[object_id] = handle;
	} else {
		object_id = int(counters.size());
		counters.emplace_back(1u); // TODO: 0 ?
		handles.emplace_back(handle);
	}

	return VulkanObjectId(device_id, object_id);
}

void VulkanObjectManager::acquire(VulkanObjectId id) {
	PASSERT(id);
	counters[id.objectId()]++;
}

void VulkanObjectManager::release(VulkanObjectId id) {
	PASSERT(id);
	int object_id = id.objectId();
	if(--counters[object_id] == 0) {
		auto &tbr_lists = to_be_released_lists[id.deviceId()];
		counters[object_id] = tbr_lists.back();
		tbr_lists.back() = object_id;
	}
}

void VulkanObjectManager::nextReleasePhase(VDeviceId device_id, VkDevice device) {
	auto &tbr_lists = to_be_released_lists[device_id];

	if(tbr_lists.front() != empty_node) {
		void (*destroy_func)(void *handle, VkDevice device) = nullptr;
#define SIMPLE_FUNC(type_id, type, func_name)                                                      \
	case VTypeId::type_id:                                                                         \
		destroy_func = [](void *handle, VkDevice device) {                                         \
			return func_name(device, (type)handle, nullptr);                                       \
		};                                                                                         \
		break;

		switch(type_id) {
			SIMPLE_FUNC(buffer, VkBuffer, vkDestroyBuffer)
			SIMPLE_FUNC(fence, VkFence, vkDestroyFence)
			SIMPLE_FUNC(semaphore, VkSemaphore, vkDestroySemaphore)
		}

#undef SIMPLE_FUNC

		int object_id = tbr_lists.front();
		while(object_id != empty_node) {
			object_id = counters[object_id];
			destroy_func(handles[object_id], device);
			handles[object_id] = nullptr;
			counters[object_id] = free_list;
			free_list = object_id;
		}
	}
	for(int i = 1; i < tbr_lists.size(); i++)
		tbr_lists[i - 1] = tbr_lists[i];
	tbr_lists.back() = empty_node;
}
}
