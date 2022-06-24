// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_device.h"

#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_storage.h"
#include "vulkan/vulkan.h"

namespace fwk {

Ex<PVSemaphore> VulkanDevice::createSemaphore(const VkSemaphoreCreateInfo &ci) {
	VkSemaphore handle;
	if(vkCreateSemaphore(m_handle, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("Failed to create semaphore");
	return g_vk_storage.allocObject<VkSemaphore>(ref(), handle);
}

Ex<PVFence> VulkanDevice::createFence(const VkFenceCreateInfo &ci) {
	VkFence handle;
	if(vkCreateFence(m_handle, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("Failed to create fence");
	return g_vk_storage.allocObject<VkFence>(ref(), handle);
}

Ex<PVShaderModule> VulkanDevice::createShaderModule(CSpan<char> bytecode) {
	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = bytecode.size();
	// TODO: make sure that this is safe
	ci.pCode = reinterpret_cast<const uint32_t *>(bytecode.data());

	VkShaderModule handle;
	if(vkCreateShaderModule(m_handle, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateShaderModule failed");
	return g_vk_storage.allocObject<VkShaderModule>(ref(), handle);
}

VulkanDevice::VulkanDevice(VDeviceId id, VPhysicalDeviceId phys_id, VInstanceRef instance_ref)
	: m_id(id), m_phys_id(phys_id), m_instance_ref(instance_ref) {
	ASSERT(m_instance_ref->valid(m_phys_id));
}

Ex<void> VulkanDevice::initialize(const VulkanDeviceSetup &setup) {
	EXPECT(!setup.queues.empty());

	vector<VkDeviceQueueCreateInfo> queue_cis;
	queue_cis.reserve(setup.queues.size());

	float default_priority = 1.0;
	for(auto &queue : setup.queues) {
		VkDeviceQueueCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		ci.queueCount = queue.count;
		ci.queueFamilyIndex = queue.family_id;
		ci.pQueuePriorities = &default_priority;
		queue_cis.emplace_back(ci);
	}

	auto swap_chain_ext = "VK_KHR_swapchain";
	vector<const char *> exts = transform(setup.extensions, [](auto &str) { return str.c_str(); });
	if(!anyOf(setup.extensions, swap_chain_ext))
		exts.emplace_back("VK_KHR_swapchain");

	VkPhysicalDeviceFeatures features{};
	if(setup.features)
		features = *setup.features;

	VkDeviceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pQueueCreateInfos = queue_cis.data();
	ci.queueCreateInfoCount = queue_cis.size();
	ci.pEnabledFeatures = &features;
	ci.enabledExtensionCount = exts.size();
	ci.ppEnabledExtensionNames = exts.data();

	m_phys_handle = m_instance_ref->info(m_phys_id).handle;
	auto result = vkCreateDevice(m_phys_handle, &ci, nullptr, &m_handle);
	if(result != VK_SUCCESS)
		return ERROR("Error during vkCreateDevice: 0x%x", stdFormat("%x", result));

	m_queues.reserve(setup.queues.size());
	for(auto &queue_def : setup.queues) {
		for(int i : intRange(queue_def.count)) {
			VkQueue queue = nullptr;
			vkGetDeviceQueue(m_handle, queue_def.family_id, i, &queue);
			m_queues.emplace_back(queue, queue_def.family_id);
		}
	}

	return {};
}

VulkanDevice::~VulkanDevice() {
	// TODO: make sure that all the objects are properly destroyed?
	if(m_handle) {
		vkDeviceWaitIdle(m_handle);
		// Destroying objects
		for(int i = 0; i < 3; i++)
			nextReleasePhase();
		vkDestroyDevice(m_handle, nullptr);
	}
}

void VulkanDevice::nextReleasePhase() { g_vk_storage.nextReleasePhase(m_id, m_handle); }
}