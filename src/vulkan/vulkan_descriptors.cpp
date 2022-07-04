// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_descriptors.h"

#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_pipeline.h"

#include <vulkan/vulkan.h>

namespace fwk {

VulkanSampler::VulkanSampler(VkSampler handle, VObjectId id, const VSamplingParams &params)
	: VulkanObjectBase(handle, id), m_params(params) {}
VulkanSampler::~VulkanSampler() { deferredHandleRelease<VkSampler, vkDestroySampler>(); }

void DescriptorSet::update(CSpan<Assignment> assigns) {
	DASSERT(assigns.size() <= max_assignments);

	array<VkDescriptorBufferInfo, max_assignments> buffer_infos;
	array<VkDescriptorImageInfo, max_assignments> image_infos;
	array<VkWriteDescriptorSet, max_assignments> writes;
	int num_buffers = 0, num_images = 0;

	for(int i : intRange(assigns)) {
		auto &write = writes[i];
		auto &assign = assigns[i];
		write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstBinding = assign.binding;
		write.dstSet = handle;
		write.descriptorType = toVk(assign.type);
		write.descriptorCount = 1;
		if(const PVBuffer *pbuffer = assign.data) {
			auto &bi = buffer_infos[num_buffers++];
			bi = {};
			bi.buffer = *pbuffer;
			bi.offset = 0;
			bi.range = VK_WHOLE_SIZE;
			write.pBufferInfo = &bi;
		} else if(const Pair<PVSampler, PVImageView> *pair = assign.data) {
			auto &ii = image_infos[num_images++];
			ii = {};
			ii.imageView = pair->second;
			ii.sampler = pair->first;
			ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			write.pImageInfo = &ii;
		}
	}
	vkUpdateDescriptorSets(pool->deviceHandle(), assigns.size(), writes.data(), 0, nullptr);
}

Ex<DescriptorSet> VulkanDescriptorPool::alloc(PVPipelineLayout layout, uint index) {
	VkDescriptorSetLayout layout_handle = layout->descriptorSetLayouts()[index];
	VkDescriptorSetAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = m_handle;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &layout_handle;
	VkDescriptorSet handle;
	auto result = vkAllocateDescriptorSets(deviceHandle(), &ai, &handle);
	if(result != VK_SUCCESS)
		return ERROR("vkAllocateDescriptorSets failed");
	return DescriptorSet(layout, index, ref(), handle);
}

VulkanDescriptorPool::VulkanDescriptorPool(VkDescriptorPool handle, VObjectId id, uint max_sets)
	: VulkanObjectBase(handle, id), m_max_sets(max_sets) {}
VulkanDescriptorPool ::~VulkanDescriptorPool() {
	deferredHandleRelease<VkDescriptorPool, vkDestroyDescriptorPool>();
}

}