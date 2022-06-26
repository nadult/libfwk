// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_device.h"

#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "fwk/vulkan/vulkan_storage.h"

#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_framebuffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_shader.h"

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

template <class THandle> Pair<void *, VObjectId> VulkanDevice::allocObject() {
	auto type_id = VulkanTypeInfo<THandle>::type_id;
	using Object = typename VulkanTypeInfo<THandle>::Wrapper;

	auto &slabs = m_slabs[type_id];
	auto &slab_list = m_fillable_slabs[type_id];
	if(slab_list.empty()) {
		u32 slab_id = u32(slabs.size());
		slab_list.emplace_back(slab_id);

		// Object index 0 is reserved as invalid
		u32 usage_bits = slab_id == 0 ? 1u : 0u;
		slabs.emplace_back(usage_bits, new SlabData<Object>);
	}

	int slab_idx = slab_list.back();
	auto &slab = slabs[slab_idx];
	int local_idx = countTrailingZeros(~slab.usage_bits);
	slab.usage_bits |= (1u << local_idx);
	int object_id = local_idx + (slab_idx << slab_size_log2);
	auto *slab_data = reinterpret_cast<SlabData<Object> *>(slab.data);
	return {&slab_data->objects[local_idx], VObjectId(m_id, object_id)};
}

template <class TObject> void VulkanDevice::destroyObject(VulkanObjectBase<TObject> *ptr) {
	using Handle = typename VulkanObjectBase<TObject>::Handle;
	auto type_id = VulkanTypeInfo<Handle>::type_id;
	uint object_idx = uint(ptr->objectId().objectIdx());

	reinterpret_cast<TObject *>(ptr)->~TObject();

	uint local_idx = object_idx & ((1 << slab_size_log2) - 1);
	uint slab_idx = object_idx >> slab_size_log2;
	auto &slab = m_slabs[type_id][slab_idx];
	if(slab.usage_bits == ~0u)
		m_fillable_slabs[type_id].emplace_back(slab_idx);
	slab.usage_bits &= ~(1u << local_idx);
}

template <class T> void VulkanObjectBase<T>::destroyObject() {
	auto &device = reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[deviceId()]);
	device.destroyObject(this);
}

template <class T> void VulkanObjectBase<T>::deferredHandleRelease(ReleaseFunc release_func) {
	auto &device = reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[deviceId()]);
	device.deferredRelease(m_handle, release_func);
}

#define CASE_TYPE(UpperCase, _)                                                                    \
	template Pair<void *, VObjectId> VulkanDevice::allocObject<Vk##UpperCase>();                   \
	template void VulkanDevice::destroyObject(VulkanObjectBase<Vulkan##UpperCase> *);              \
	template void VulkanObjectBase<Vulkan##UpperCase>::destroyObject();                            \
	template void VulkanObjectBase<Vulkan##UpperCase>::deferredHandleRelease(ReleaseFunc);
#include "fwk/vulkan/vulkan_type_list.h"
}