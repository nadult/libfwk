// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_map.h"
#include "fwk/hash_map.h"
#include "fwk/sparse_vector.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"

#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanDescriptorManager {
  public:
	VulkanDescriptorManager(VkDevice);
	FWK_IMMOVABLE_CLASS(VulkanDescriptorManager);

	VDSLId getLayout(CSpan<VDescriptorBindingInfo>);
	VkDescriptorSetLayout handle(VDSLId dsl_id) const { return m_dsls[dsl_id].layout; }

	// This should only be called between beginFrame & finishFrame
	VkDescriptorSet acquireSet(VDSLId);

	u64 bindingMap(VDSLId) const;
	CSpan<VDescriptorBindingInfo> bindings(VDSLId) const;

	void beginFrame(uint swap_frame_index);
	void finishFrame();

  private:
	VkDescriptorPool allocPool(CSpan<VDescriptorBindingInfo>, uint num_sets);
	void allocSets(VkDescriptorPool, VkDescriptorSetLayout, Span<VkDescriptorSet>);

	struct HashedDSL {
		HashedDSL(CSpan<VDescriptorBindingInfo> bindings, Maybe<u32> hash_value)
			: bindings(bindings),
			  hash_value(hash_value.orElse(VDescriptorBindingInfo::hashIgnoreSet(bindings))) {}

		u32 hash() const { return hash_value; }
		bool operator==(const HashedDSL &rhs) const {
			return hash_value == rhs.hash_value && bindings == rhs.bindings;
		}

		CSpan<VDescriptorBindingInfo> bindings;
		u32 hash_value;
	};

	struct DSL {
		static constexpr int num_initial_sets = 10;

		uint first_binding, num_bindings; // TODO: binding_infos ?
		u64 binding_map = 0;
		VkDescriptorSetLayout layout = nullptr;
		VkDescriptorPool pool = nullptr;
		uint num_allocated = 0, num_used = 0;
		VkDescriptorSet handles[num_initial_sets];
		VkDescriptorSet *more_handles = nullptr;
	};

	VkDevice m_device_handle;
	vector<VDescriptorBindingInfo> m_declarations;
	vector<DSL> m_dsls;
	vector<VkDescriptorPool> m_deferred_releases[VulkanLimits::num_swap_frames];
	HashMap<HashedDSL, VDSLId> m_hash_map;

	uint m_swap_frame_index = 0;
	bool m_frame_running = false;
};

}