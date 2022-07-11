// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_descriptor_manager.h"

#include "fwk/index_range.h"
#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_shader.h"

#include <vulkan/vulkan.h>

// TODO: free descriptors if they aren't used for several frames
// TODO: more concise form of descriptor declarations
// TODO: properly handling errors
// TODO: we can have 12 handles in DSL with union with a ptr

// TODO: better name for Descriptor Set;
// ShaderControl ? Board ?  Input / InputLayout ?
// ShaderInput, ShaderInputLayout ?

namespace fwk {

VulkanDescriptorManager::VulkanDescriptorManager(VkDevice device) : m_device_handle(device) {
	static_assert(sizeof(VulkanDescriptorManager::DSL) == 128);
}

VulkanDescriptorManager::~VulkanDescriptorManager() {
	for(auto &dsl : m_dsls) {
		if(dsl.num_allocated > DSL::num_initial_sets)
			delete[] dsl.more_handles;
		vkDestroyDescriptorPool(m_device_handle, dsl.pool, nullptr);
		vkDestroyDescriptorSetLayout(m_device_handle, dsl.layout, nullptr);
	}
}

Ex<VDSLId> VulkanDescriptorManager::getLayout(CSpan<DescriptorBindingInfo> bindings) {
	HashedDSL key(bindings, none);
	auto it = m_hash_map.find(key);
	if(it != m_hash_map.end())
		return it->value;

	auto vk_bindings = transform(bindings, [](DescriptorBindingInfo binding) {
		VkDescriptorSetLayoutBinding lb{};
		lb.binding = binding.binding();
		lb.descriptorType = toVk(binding.type());
		lb.descriptorCount = binding.count();
		lb.stageFlags = toVk(binding.stages());
		return lb;
	});

	VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	ci.pBindings = vk_bindings.data();
	ci.bindingCount = vk_bindings.size();

	DSL new_dsl;
	if(vkCreateDescriptorSetLayout(m_device_handle, &ci, nullptr, &new_dsl.layout) != VK_SUCCESS)
		return ERROR("vkCreateDescriptorSetLayout failed");
	new_dsl.pool = EX_PASS(allocPool(bindings, DSL::num_initial_sets));
	EXPECT(allocSets(new_dsl.pool, new_dsl.layout, new_dsl.handles));
	new_dsl.first_binding = m_declarations.size();
	new_dsl.num_bindings = bindings.size();
	new_dsl.num_allocated = DSL::num_initial_sets;

	auto *old_decl_data = m_declarations.data();
	insertBack(m_declarations, bindings);
	if(m_declarations.data() != old_decl_data) {
		HashMap<HashedDSL, VDSLId> new_hash_map;
		// TODO: how to pick best cap ?
		new_hash_map.reserve(m_hash_map.capacity());
		for(auto [key, value] : m_hash_map) {
			auto &dsl = m_dsls[value];
			CSpan<DescriptorBindingInfo> new_bindings(m_declarations.data() + dsl.first_binding,
													  dsl.num_bindings);
			new_hash_map.emplace({new_bindings, key.hash_value}, value);
		}
		m_hash_map.swap(new_hash_map);
	}
	bindings = cspan(m_declarations.data() + new_dsl.first_binding, new_dsl.num_bindings);
	VDSLId dsl_id(m_dsls.size());
	m_dsls.emplace_back(move(new_dsl));
	m_hash_map.emplace({bindings, key.hash_value}, dsl_id);
	return dsl_id;
}

VkDescriptorSet VulkanDescriptorManager::acquireSet(VDSLId dsl_id) {
	DASSERT(m_frame_running);
	auto &dsl = m_dsls[dsl_id];

	static_assert(VulkanLimits::num_swap_frames == 2,
				  "Fix allocation system for more than 2 swap frames");
	uint num_frame_allocated = dsl.num_allocated >> 1;

	if(dsl.num_used == num_frame_allocated) {
		uint new_alloc_size = dsl.num_allocated * 2;
		auto new_pool = allocPool(bindings(dsl_id), new_alloc_size);
		if(!new_pool) {
			m_error = move(new_pool.error());
			// TODO: What should we do now?
			return nullptr;
		}

		if(dsl.num_used > DSL::num_initial_sets)
			delete[] dsl.more_handles;
		dsl.more_handles = new VkDescriptorSet[new_alloc_size];
		auto result = allocSets(*new_pool, dsl.layout, span(dsl.more_handles, new_alloc_size));
		if(!result) {
			m_error = move(result.error());
			vkDestroyDescriptorPool(m_device_handle, *new_pool, nullptr);
			return nullptr;
		}

		m_deferred_releases[m_swap_frame_index].emplace_back(dsl.pool);
		dsl.pool = *new_pool;
		dsl.num_allocated = new_alloc_size;
		num_frame_allocated = new_alloc_size >> 1;
	}

	uint index = dsl.num_used++ + m_swap_frame_index * num_frame_allocated;
	return dsl.num_allocated <= DSL::num_initial_sets ? dsl.handles[index] :
														  dsl.more_handles[index];
}

CSpan<DescriptorBindingInfo> VulkanDescriptorManager::bindings(VDSLId dsl_id) const {
	auto &dsl = m_dsls[dsl_id];
	return {m_declarations.data() + dsl.first_binding, int(dsl.num_bindings)};
}

void VulkanDescriptorManager::beginFrame(uint swap_frame_index) {
	DASSERT(!m_frame_running);
	m_frame_running = true;

	m_swap_frame_index = swap_frame_index;
	for(auto pool_handle : m_deferred_releases[swap_frame_index])
		vkDestroyDescriptorPool(m_device_handle, pool_handle, nullptr);
	m_deferred_releases[swap_frame_index].clear();
	for(auto &dsl : m_dsls)
		dsl.num_used = 0;
}

void VulkanDescriptorManager::finishFrame() {
	DASSERT(m_frame_running);
	m_frame_running = false;
}

Ex<VkDescriptorPool> VulkanDescriptorManager::allocPool(CSpan<DescriptorBindingInfo> bindings,
														uint num_sets) {
	EnumMap<VDescriptorType, uint> counts;
	for(auto binding : bindings)
		counts[binding.type()] += binding.count();

	array<VkDescriptorPoolSize, count<VDescriptorType>> sizes;
	uint num_sizes = 0;
	for(auto type : all<VDescriptorType>)
		if(counts[type] > 0)
			sizes[num_sizes++] = {toVk(type), counts[type] * num_sets};

	VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	ci.poolSizeCount = num_sizes;
	ci.pPoolSizes = sizes.data();
	ci.maxSets = num_sets;
	ci.flags = 0; //TODO
	VkDescriptorPool handle;
	if(vkCreateDescriptorPool(m_device_handle, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateDescriptorPool failed");
	return handle;
}

Ex<void> VulkanDescriptorManager::allocSets(VkDescriptorPool pool, VkDescriptorSetLayout layout,
											Span<VkDescriptorSet> out) {
	static constexpr int step_size = 16;
	array<VkDescriptorSetLayout, step_size> layouts;
	fill(layouts, layout);

	VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	ai.descriptorPool = pool;
	ai.pSetLayouts = layouts.data();
	for(int i = 0; i < out.size(); i += step_size) {
		ai.descriptorSetCount = min(out.size() - i, step_size);
		auto result = vkAllocateDescriptorSets(m_device_handle, &ai, &out[i]);
		if(result != VK_SUCCESS)
			return ERROR("vkAllocateDescriptorSets failed");
	}
	return {};
}

}