// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanSampler : public VulkanObjectBase<VulkanSampler> {
  public:
	const auto &params() const { return m_params; }

  private:
	friend class VulkanDevice;
	VulkanSampler(VkSampler, VObjectId, const VSamplingParams &);
	~VulkanSampler();

	VSamplingParams m_params;
};

struct DescriptorPoolSetup {
	EnumMap<VDescriptorType, uint> sizes;
	uint max_sets = 1;
};

// TODO: would it be better to turn it into managed class?
struct DescriptorSet {
	DescriptorSet(PVPipelineLayout layout, uint layout_index, PVDescriptorPool pool,
				  VkDescriptorSet handle)
		: layout(layout), layout_index(layout_index), pool(pool), handle(handle) {}
	DescriptorSet() {}

	struct Assignment {
		VDescriptorType type;
		uint binding;
		Variant<Pair<PVSampler, PVImageView>, PVBuffer> data;
	};

	static constexpr int max_assignments = 16;
	void update(CSpan<Assignment>);

	PVPipelineLayout layout;
	uint layout_index = 0;

	PVDescriptorPool pool;
	VkDescriptorSet handle = nullptr;
};

class VulkanDescriptorPool : public VulkanObjectBase<VulkanDescriptorPool> {
  public:
	Ex<DescriptorSet> alloc(PVPipelineLayout, uint index);

  private:
	friend class VulkanDevice;
	VulkanDescriptorPool(VkDescriptorPool, VObjectId, uint max_sets);
	~VulkanDescriptorPool();

	uint m_num_sets = 0, m_max_sets;
};

}