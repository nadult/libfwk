// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_device.h"

#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_memory_manager.h"
#include "fwk/vulkan/vulkan_render_graph.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "fwk/vulkan/vulkan_storage.h"

#include "fwk/format.h"
#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_descriptors.h"
#include "fwk/vulkan/vulkan_framebuffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "fwk/vulkan/vulkan_swap_chain.h"

#include <vulkan/vulkan.h>

namespace fwk {

VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBuffer handle, VObjectId id, PVCommandPool pool)
	: VulkanObjectBase(handle, id), m_pool(move(pool)) {}

VulkanCommandBuffer ::~VulkanCommandBuffer() {
	// CommandPool should be destroyed after all CommandBuffers belonging to it
	deferredHandleRelease(m_handle, m_pool.handle(), [](void *p0, void *p1, VkDevice device) {
		VkCommandBuffer handles[1] = {(VkCommandBuffer)p0};
		vkFreeCommandBuffers(device, (VkCommandPool)p1, 1, handles);
	});
}

VulkanCommandPool::VulkanCommandPool(VkCommandPool handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}
VulkanCommandPool ::~VulkanCommandPool() {
	deferredHandleRelease<VkCommandPool, vkDestroyCommandPool>();
}

Ex<PVCommandBuffer> VulkanCommandPool::allocBuffer() {
	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = m_handle;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = 1;

	auto device = deviceRef();

	VkCommandBuffer handle;
	if(vkAllocateCommandBuffers(device, &ai, &handle) != VK_SUCCESS)
		return ERROR("vkAllocateCommandBuffers failed");
	return device->createObject(handle, ref());
}

VulkanFence::VulkanFence(VkFence handle, VObjectId id) : VulkanObjectBase(handle, id) {}
VulkanFence ::~VulkanFence() { deferredHandleRelease<VkFence, vkDestroyFence>(); }

VulkanSemaphore::VulkanSemaphore(VkSemaphore handle, VObjectId id) : VulkanObjectBase(handle, id) {}
VulkanSemaphore ::~VulkanSemaphore() { deferredHandleRelease<VkSemaphore, vkDestroySemaphore>(); }

// -------------------------------------------------------------------------------------------
// ------  Implementation struct declarations  -----------------------------------------------

static constexpr int object_pool_size = 32;
static constexpr int object_pool_size_log2 = 5;

template <class T> struct PoolData {
	std::aligned_storage_t<sizeof(T), alignof(T)> objects[object_pool_size];
};

struct VulkanDevice::ObjectPools {
	// TODO: trim empty unused pools from time to time we can't move them around, but we can free the pool and
	// leave nullptr in pool array. Maybe just delete pool data every time when it's usage_bits are empty?

	struct Pool {
		u32 usage_bits = 0;
		void *data = nullptr;
	};

	struct DeferredRelease {
		void *param0;
		void *param1;
		ReleaseFunc func;
	};

	EnumMap<VTypeId, vector<Pool>> pools;
	EnumMap<VTypeId, vector<u32>> fillable_pools;
	array<vector<DeferredRelease>, max_defer_frames + 1> to_release;
};

// -------------------------------------------------------------------------------------------
// ---  Construction / destruction & main functions  -----------------------------------------

VulkanDevice::VulkanDevice(VDeviceId id, VPhysicalDeviceId phys_id, VInstanceRef instance_ref)
	: m_id(id), m_phys_id(phys_id), m_instance_ref(instance_ref) {
	m_objects.emplace();
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

	const auto &phys_info = m_instance_ref->info(m_phys_id);
	auto exts = setup.extensions;
	exts.emplace_back("VK_KHR_swapchain");
	if(anyOf(phys_info.extensions, "VK_EXT_memory_budget")) {
		m_features |= VDeviceFeature::memory_budget;
		exts.emplace_back("VK_EXT_memory_budget");
		if(m_instance_ref->version() < VulkanVersion{1, 1, 0})
			exts.emplace_back("VK_KHR_get_physical_device_properties2");
	}
	makeSortedUnique(exts);

	VkPhysicalDeviceFeatures features{};
	if(setup.features)
		features = *setup.features;

	vector<const char *> c_exts = transform(exts, [](auto &str) { return str.c_str(); });
	VkDeviceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pQueueCreateInfos = queue_cis.data();
	ci.queueCreateInfoCount = queue_cis.size();
	ci.pEnabledFeatures = &features;
	ci.enabledExtensionCount = c_exts.size();
	ci.ppEnabledExtensionNames = c_exts.data();

	m_phys_handle = phys_info.handle;
	auto result = vkCreateDevice(m_phys_handle, &ci, nullptr, &m_handle);
	if(result != VK_SUCCESS)
		return ERROR("Error during vkCreateDevice: 0x%x", stdFormat("%x", result));
	g_vk_storage.device_handles[m_id] = m_handle;

	m_queues.reserve(setup.queues.size());
	for(auto &queue_def : setup.queues) {
		for(int i : intRange(queue_def.count)) {
			VkQueue queue = nullptr;
			vkGetDeviceQueue(m_handle, queue_def.family_id, i, &queue);
			m_queues.emplace_back(queue, queue_def.family_id);
		}
	}

	m_memory.emplace(m_handle, m_instance_ref->info(m_phys_id), m_features);
	return {};
}

VulkanDevice::~VulkanDevice() {
	if(m_handle)
		vkDeviceWaitIdle(m_handle);
	m_render_graph.reset();

#ifndef NDEBUG
	bool errors = false;
	for(auto type_id : all<VTypeId>) {
		auto &pools = m_objects->pools[type_id];
		uint num_objects = 0;
		for(int i : intRange(pools)) {
			auto usage_bits = pools[i].usage_bits;
			if(i == 0)
				usage_bits &= ~1u;
			num_objects += countBits(usage_bits);
		}

		if(num_objects > 0) {
			if(!errors) {
				print("Errors when destroying VulkanDevice:\n");
				errors = true;
			}
			print("% object% of type '%' still exist\n", num_objects, num_objects > 1 ? "s" : "",
				  type_id);
		}
	}
	if(errors)
		FATAL("All VPtrs<> to Vulkan objects should be freed before VulkanDevice is destroyed");
#endif
	m_memory.reset();

	if(m_handle) {
		for(int i = 0; i <= max_defer_frames; i++)
			nextReleasePhase();
		g_vk_storage.device_handles[m_id] = nullptr;
		vkDestroyDevice(m_handle, nullptr);
	}
}

Ex<void> VulkanDevice::createRenderGraph(PVSwapChain swap_chain) {
	DASSERT(!m_render_graph && "Render graph already initialized");
	m_render_graph = new VulkanRenderGraph(ref());
	return m_render_graph->initialize(ref(), swap_chain);
}

Ex<void> VulkanDevice::beginFrame() {
	if(m_render_graph)
		EXPECT(m_render_graph->beginFrame());
	m_memory->beginFrame();
	return {};
}

Ex<void> VulkanDevice::finishFrame() {
	m_memory->finishFrame();
	if(m_render_graph)
		EXPECT(m_render_graph->finishFrame());
	return {};
}

// -------------------------------------------------------------------------------------------
// ----------  Object management  ------------------------------------------------------------

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

Ex<PVCommandPool> VulkanDevice::createCommandPool(VQueueFamilyId queue_family_id,
												  VCommandPoolFlags flags) {
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = toVk(flags);
	poolInfo.queueFamilyIndex = queue_family_id;
	VkCommandPool handle;
	if(vkCreateCommandPool(m_handle, &poolInfo, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateCommandPool failed");
	return createObject(handle);
}

Ex<PVSampler> VulkanDevice::createSampler(const VSamplingParams &params) {
	VkSamplerCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	ci.magFilter = VkFilter(params.mag_filter);
	ci.minFilter = VkFilter(params.min_filter);
	ci.mipmapMode = VkSamplerMipmapMode(params.mipmap_filter.orElse(VTexFilter::nearest));
	ci.addressModeU = VkSamplerAddressMode(params.address_mode[0]);
	ci.addressModeV = VkSamplerAddressMode(params.address_mode[1]);
	ci.addressModeW = VkSamplerAddressMode(params.address_mode[2]);
	ci.maxAnisotropy = params.max_anisotropy_samples;
	ci.anisotropyEnable = params.max_anisotropy_samples > 1;
	VkSampler handle;
	if(vkCreateSampler(m_handle, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateSampler failed");
	return createObject(handle, params);
}

Ex<PVDescriptorPool> VulkanDevice::createDescriptorPool(const DescriptorPoolSetup &setup) {
	array<VkDescriptorPoolSize, count<VDescriptorType>> sizes;
	uint num_sizes = 0;
	for(auto type : all<VDescriptorType>)
		if(setup.sizes[type] > 0)
			sizes[num_sizes++] = {.type = toVk(type), .descriptorCount = setup.sizes[type]};

	VkDescriptorPoolCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	ci.poolSizeCount = num_sizes;
	ci.pPoolSizes = sizes.data();
	ci.maxSets = setup.max_sets;
	ci.flags = 0; //TODO

	VkDescriptorPool handle;
	if(vkCreateDescriptorPool(m_handle, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateDescriptorPool failed");
	return createObject(handle, setup.max_sets);
}

using ReleaseFunc = void (*)(void *, void *, VkDevice);
void VulkanDevice::deferredRelease(void *param0, void *param1, ReleaseFunc func, int num_frames) {
	DASSERT(num_frames <= max_defer_frames);
	m_objects->to_release[num_frames].emplace_back(param0, param1, func);
}

void VulkanDevice::nextReleasePhase() {
	auto &to_release = m_objects->to_release;
	for(auto [param0, param1, func] : to_release[0])
		func(param0, param1, m_handle);
	to_release[0].clear();
	for(int i = 1; i <= max_defer_frames; i++)
		to_release[i - 1].swap(to_release[i]);
}

Ex<VMemoryBlock> VulkanDevice::alloc(VMemoryUsage usage, const VkMemoryRequirements &requirements) {
	return m_memory->alloc(usage, requirements);
}

template <class TObject> Pair<void *, VObjectId> VulkanDevice::allocObject() {
	auto type_id = VulkanTypeInfo<TObject>::type_id;

	auto &pools = m_objects->pools[type_id];
	auto &pool_list = m_objects->fillable_pools[type_id];
	if(pool_list.empty()) {
		u32 pool_id = u32(pools.size());
		pool_list.emplace_back(pool_id);

		// Object index 0 is reserved as invalid
		u32 usage_bits = pool_id == 0 ? 1u : 0u;
		pools.emplace_back(usage_bits, new PoolData<TObject>);
	}

	int pool_idx = pool_list.back();
	auto &pool = pools[pool_idx];
	int local_idx = countTrailingZeros(~pool.usage_bits);
	pool.usage_bits |= (1u << local_idx);
	int object_id = local_idx + (pool_idx << object_pool_size_log2);
	auto *pool_data = reinterpret_cast<PoolData<TObject> *>(pool.data);
	return {&pool_data->objects[local_idx], VObjectId(m_id, object_id)};
}

template <class TObject> void VulkanDevice::destroyObject(VulkanObjectBase<TObject> *ptr) {
	auto type_id = VulkanTypeInfo<TObject>::type_id;
	uint object_idx = uint(ptr->objectId().objectIdx());

	reinterpret_cast<TObject *>(ptr)->~TObject();

	uint local_idx = object_idx & ((1 << object_pool_size_log2) - 1);
	uint pool_idx = object_idx >> object_pool_size_log2;
	auto &pool = m_objects->pools[type_id][pool_idx];
	if(pool.usage_bits == ~0u)
		m_objects->fillable_pools[type_id].emplace_back(pool_idx);
	pool.usage_bits &= ~(1u << local_idx);
}

template <class T> void VulkanObjectBase<T>::destroyObject() {
	auto &device = reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[deviceId()]);
	device.destroyObject(this);
}

template <class T>
void VulkanObjectBase<T>::deferredHandleRelease(void *p0, void *p1, ReleaseFunc release_func) {
	auto &device = reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[deviceId()]);
	device.deferredRelease(p0, p1, release_func);
}

#define CASE_TYPE(UpperCase, VkName, _)                                                            \
	template Pair<void *, VObjectId> VulkanDevice::allocObject<Vulkan##UpperCase>();               \
	template void VulkanDevice::destroyObject(VulkanObjectBase<Vulkan##UpperCase> *);              \
	template void VulkanObjectBase<Vulkan##UpperCase>::destroyObject();                            \
	template void VulkanObjectBase<Vulkan##UpperCase>::deferredHandleRelease(void *, void *,       \
																			 ReleaseFunc);
#include "fwk/vulkan/vulkan_type_list.h"

}