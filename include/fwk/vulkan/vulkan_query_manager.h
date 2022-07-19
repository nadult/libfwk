// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VQuery {
	uint index;
	uint pool_index;
	VkQueryPool pool_handle;
};

// For now only timestamp queries are supported
class VulkanQueryManager {
  public:
	VulkanQueryManager(VkDevice);
	FWK_IMMOVABLE_CLASS(VulkanQueryManager);

	void beginFrame(u64 frame_index);
	void finishFrame();

	VQuery acquireQuery();

	// Zbieramy queriesy z konkretnj klatki
	// Poole mamy ograniczone, que
	// Perf manager musi zapisaæ

	// Returns empty vector if results are not ready
	vector<u64> getResults(u64 frame_index);

  private:
	static constexpr uint pool_shift = 10, pool_size = 1 << 10;

	struct QuerySet {
		vector<VkQueryPool> pools;
		u64 frame_index = 0;
		uint count = 0;
	};

	vector<Pair<u64, vector<u64>>> m_results;
	QuerySet m_sets[VulkanLimits::num_swap_frames];
	VkDevice m_device_handle;
	uint m_swap_index = 0;
	bool m_frame_running = false;
};

}