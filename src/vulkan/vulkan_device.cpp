// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_device.h"

#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "fwk/vulkan/vulkan_storage.h"
#include <vulkan/vulkan.h>

namespace fwk {

VulkanCommandPool::VulkanCommandPool(VkCommandPool handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}
VulkanCommandPool ::~VulkanCommandPool() {
	deferredHandleRelease([](void *handle, VkDevice device) {
		vkDestroyCommandPool(device, (VkCommandPool)handle, nullptr);
	});
}

VulkanFence::VulkanFence(VkFence handle, VObjectId id) : VulkanObjectBase(handle, id) {}
VulkanFence ::~VulkanFence() {
	deferredHandleRelease(
		[](void *handle, VkDevice device) { vkDestroyFence(device, (VkFence)handle, nullptr); });
}

VulkanSemaphore::VulkanSemaphore(VkSemaphore handle, VObjectId id) : VulkanObjectBase(handle, id) {}
VulkanSemaphore ::~VulkanSemaphore() {
	deferredHandleRelease([](void *handle, VkDevice device) {
		vkDestroySemaphore(device, (VkSemaphore)handle, nullptr);
	});
}

VulkanDeviceMemory::VulkanDeviceMemory(VkDeviceMemory handle, VObjectId id, u64 size,
									   VMemoryFlags flags)
	: VulkanObjectBase(handle, id), m_size(size), m_flags(flags) {}
VulkanDeviceMemory ::~VulkanDeviceMemory() {
	deferredHandleRelease([](void *handle, VkDevice device) {
		vkFreeMemory(device, (VkDeviceMemory)handle, nullptr);
	});
}

static const EnumMap<VMemoryFlag, VkMemoryPropertyFlagBits> memory_flags = {{
	{VMemoryFlag::device_local, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
	{VMemoryFlag::host_visible, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT},
	{VMemoryFlag::host_coherent, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT},
	{VMemoryFlag::host_cached, VK_MEMORY_PROPERTY_HOST_CACHED_BIT},
	{VMemoryFlag::lazily_allocated, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT},
	{VMemoryFlag::protected_, VK_MEMORY_PROPERTY_PROTECTED_BIT},
}};

Ex<PVSemaphore> VulkanDevice::createSemaphore(bool is_signaled) {
	VkSemaphore handle;
	VkSemaphoreCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	ci.flags = is_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
	if(vkCreateSemaphore(m_handle, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("Failed to create semaphore");
	return createObject(handle);
}

Ex<PVFence> VulkanDevice::createFence(bool is_signaled) {
	VkFence handle;
	VkFenceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	ci.flags = is_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
	if(vkCreateFence(m_handle, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("Failed to create fence");
	return createObject(handle);
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
	return createObject(handle);
}

Ex<PVDeviceMemory> VulkanDevice::allocDeviceMemory(u64 size, u32 memory_type_bits,
												   VMemoryFlags flags) {
	auto &phys_info = m_instance_ref->info(m_phys_id);
	auto vk_flags = translateFlags(flags, memory_flags);
	auto mem_type = phys_info.findMemoryType(memory_type_bits, vk_flags);
	if(!mem_type)
		return ERROR("Couldn't find a suitable memory type; bits:% flags:%", memory_type_bits,
					 flags);

	VkMemoryAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = size;
	ai.memoryTypeIndex = *mem_type;
	VkDeviceMemory handle;
	if(vkAllocateMemory(m_handle, &ai, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkAllocateMemory failed");
	return createObject(handle, size, flags);
}

static const EnumMap<CommandPoolFlag, VkCommandPoolCreateFlagBits> command_pool_flags = {{
	{CommandPoolFlag::transient, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT},
	{CommandPoolFlag::reset_command, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT},
	{CommandPoolFlag::protected_, VK_COMMAND_POOL_CREATE_PROTECTED_BIT},
}};

Ex<PVCommandPool> VulkanDevice::createCommandPool(VQueueFamilyId queue_family_id,
												  CommandPoolFlags flags) {
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = 0;
	poolInfo.queueFamilyIndex = queue_family_id;
	for(auto flag : flags)
		poolInfo.flags |= command_pool_flags[flag];
	VkCommandPool handle;
	if(vkCreateCommandPool(m_handle, &poolInfo, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateCommandPool failed");
	return createObject(handle);
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

	g_vk_storage.device_handles[m_id] = m_handle;

	return {};
}

VulkanDevice::~VulkanDevice() {
	if(m_handle) {
		g_vk_storage.device_handles[m_id] = nullptr;
		vkDeviceWaitIdle(m_handle);
		for(int i = 0; i <= max_defer_frames; i++)
			nextReleasePhase();
		vkDestroyDevice(m_handle, nullptr);
	}
}

using ReleaseFunc = void (*)(void *, VkDevice);
void VulkanDevice::deferredRelease(void *ptr, ReleaseFunc func, int num_frames) {
	DASSERT(num_frames <= max_defer_frames);
	m_to_release[num_frames].emplace_back(ptr, func);
}

void VulkanDevice::nextReleasePhase() {
	for(auto [ptr, func] : m_to_release[0])
		func(ptr, m_handle);
	m_to_release[0].clear();
	for(int i = 1; i <= max_defer_frames; i++)
		m_to_release[i - 1].swap(m_to_release[i]);
}

//#define CASE_TYPE(UpperCase, _)                                                                    \
	template void VulkanStorage::decRef<Vk##UpperCase>(VObjectId);                                 \
//#include "fwk/vulkan/vulkan_type_list.h"

}