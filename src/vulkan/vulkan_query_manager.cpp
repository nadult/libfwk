// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_query_manager.h"

#include "fwk/index_range.h"
#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_shader.h"

namespace fwk {

VulkanQueryManager::VulkanQueryManager(VkDevice device) : m_device_handle(device) {}

VulkanQueryManager::~VulkanQueryManager() {
	for(auto &set : m_sets)
		for(auto pool : set.pools)
			vkDestroyQueryPool(m_device_handle, pool, nullptr);
}

void VulkanQueryManager::beginFrame(u64 frame_index) {
	PASSERT(!m_frame_running);
	m_swap_index = frame_index % VulkanLimits::num_swap_frames;
	auto &set = m_sets[m_swap_index];

	if(set.count > 0) {
		PodVector<u64> new_data(set.count);
		uint cur_count = 0;
		for(auto pool : set.pools) {
			uint pool_count = min(set.count - cur_count, pool_size);
			if(pool_count == 0)
				break;

			size_t data_size = sizeof(u64) * pool_count;
			auto data_offset = new_data.data() + cur_count;
			cur_count += pool_count;

			FWK_VK_CALL(vkGetQueryPoolResults, m_device_handle, pool, 0, pool_count, data_size,
						data_offset, sizeof(u64), VK_QUERY_RESULT_64_BIT);
			vkResetQueryPool(m_device_handle, pool, 0, pool_count);
		}

		vector<u64> new_data_vec;
		new_data.unsafeSwap(new_data_vec);
		m_results.emplace_back(set.frame_index, move(new_data));
	}

	set.count = 0;
	set.frame_index = frame_index;
	m_frame_running = true;
}

void VulkanQueryManager::finishFrame() { PASSERT(m_frame_running); }

VQuery VulkanQueryManager::acquireQuery() {
	PASSERT(m_frame_running);
	auto &set = m_sets[m_swap_index];
	uint index = set.count++;
	uint pool_id = index >> pool_shift;
	if(pool_id >= set.pools.size()) {
		VkQueryPoolCreateInfo ci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		ci.queryCount = pool_size;
		ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
		VkQueryPool handle = nullptr;
		FWK_VK_CALL(vkCreateQueryPool, m_device_handle, &ci, nullptr, &handle);
		set.pools.emplace_back(handle);
	}
	return {index, index & (pool_size - 1), set.pools[pool_id]};
}

vector<u64> VulkanQueryManager::getResults(u64 frame_index) {
	for(int i : intRange(m_results)) {
		if(m_results[i].first == frame_index) {
			vector<u64> out = move(m_results[i].second);
			m_results[i] = move(m_results.back());
			m_results.pop_back();
			return out;
		}
	}
	return {};
}

}