// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_device.h"

#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_render_graph.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "fwk/vulkan/vulkan_storage.h"

#include "fwk/format.h"
#include "fwk/vulkan/vulkan_buffer.h"
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

VulkanDeviceMemory::VulkanDeviceMemory(VkDeviceMemory handle, VObjectId id, u64 size,
									   VMemoryFlags flags)
	: VulkanObjectBase(handle, id), m_size(size), m_flags(flags) {}
VulkanDeviceMemory ::~VulkanDeviceMemory() {
	if(m_deferred_free)
		deferredHandleRelease<VkDeviceMemory, vkFreeMemory>();
	else
		vkFreeMemory(deviceHandle(), m_handle, nullptr);
}

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

Ex<PVDeviceMemory> VulkanDevice::allocDeviceMemory(u64 size, uint memory_type_index) {
	const auto *mem_types = m_instance_ref->info(m_phys_id).mem_properties.memoryTypes;
	VMemoryFlags flags;
	flags.bits = (mem_types[memory_type_index].propertyFlags) & VMemoryFlags::mask;

	VkMemoryAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = size;
	ai.memoryTypeIndex = memory_type_index;
	VkDeviceMemory handle;
	if(vkAllocateMemory(m_handle, &ai, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkAllocateMemory failed");
	return createObject(handle, size, flags);
}

Ex<PVDeviceMemory> VulkanDevice::allocDeviceMemory(u64 size, u32 memory_type_bits,
												   VMemoryFlags flags) {
	auto &phys_info = m_instance_ref->info(m_phys_id);
	auto mem_type = phys_info.findMemoryType(memory_type_bits, flags);
	if(mem_type == -1)
		return ERROR("Couldn't find a suitable memory type; bits:% flags:%", memory_type_bits,
					 flags);
	return allocDeviceMemory(size, mem_type);
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

Ex<void> VulkanDevice::createRenderGraph(PVSwapChain swap_chain) {
	DASSERT(!m_render_graph && "Render graph already initialized");
	m_render_graph = new VulkanRenderGraph(ref());
	return m_render_graph->initialize(ref(), swap_chain);
}

static constexpr int pool_size = 32;
static constexpr int pool_size_log2 = 5;

template <class T> struct PoolData {
	std::aligned_storage_t<sizeof(T), alignof(T)> objects[pool_size];
};

struct VulkanDevice::Impl {
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

	// TODO: signify which queue is for what?
	vector<Pair<VkQueue, VQueueFamilyId>> queues;
};

VulkanDevice::VulkanDevice(VDeviceId id, VPhysicalDeviceId phys_id, VInstanceRef instance_ref)
	: m_id(id), m_phys_id(phys_id), m_instance_ref(instance_ref) {
	m_impl.emplace();
	ASSERT(m_instance_ref->valid(m_phys_id));

	auto &info = instance_ref->info(phys_id);
	auto &mem_info = info.mem_properties;
	EnumMap<VMemoryDomain, VMemoryFlags> domain_flags = {
		{VMemoryFlag::device_local, VMemoryFlag::host_visible,
		 VMemoryFlag::device_local | VMemoryFlag::host_visible}};

	for(auto domain : all<VMemoryDomain>) {
		int type_index = info.findMemoryType(~0u, domain_flags[domain]);
		int heap_index = -1;
		u64 heap_size = 0;
		if(type_index != -1) {
			heap_index = mem_info.memoryTypes[type_index].heapIndex;
			heap_size = mem_info.memoryHeaps[heap_index].size;
		}
		m_mem_domains[domain] = {type_index, heap_index, heap_size};
	}
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

	auto &queues = m_impl->queues;
	queues.reserve(setup.queues.size());
	for(auto &queue_def : setup.queues) {
		for(int i : intRange(queue_def.count)) {
			VkQueue queue = nullptr;
			vkGetDeviceQueue(m_handle, queue_def.family_id, i, &queue);
			queues.emplace_back(queue, queue_def.family_id);
		}
	}

	g_vk_storage.device_handles[m_id] = m_handle;

	return {};
}

VulkanDevice::~VulkanDevice() {
	m_render_graph.reset();

#ifndef NDEBUG
	bool errors = false;
	for(auto type_id : all<VTypeId>) {
		auto &pools = m_impl->pools[type_id];
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

	if(m_handle) {
		g_vk_storage.device_handles[m_id] = nullptr;
		vkDeviceWaitIdle(m_handle);
		for(int i = 0; i <= max_defer_frames; i++)
			nextReleasePhase();
		vkDestroyDevice(m_handle, nullptr);
	}
}

auto VulkanDevice::memoryBudget() const -> EnumMap<VMemoryDomain, MemoryBudget> {
	EnumMap<VMemoryDomain, MemoryBudget> out;

	if(m_features & VDeviceFeature::memory_budget) {
		VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
		VkPhysicalDeviceMemoryProperties2 props{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
		props.pNext = &budget;
		vkGetPhysicalDeviceMemoryProperties2(m_phys_handle, &props);

		for(auto domain : all<VMemoryDomain>) {
			int type_index = m_mem_domains[domain].type_index;
			if(type_index != -1) {
				auto heap_index = m_mem_domains[domain].heap_index;
				out[domain].heap_budget = budget.heapBudget[heap_index];
				out[domain].heap_usage = budget.heapUsage[heap_index];
			}
		}
	}
	return out;
}

CSpan<Pair<VkQueue, VQueueFamilyId>> VulkanDevice::queues() const { return m_impl->queues; }

using ReleaseFunc = void (*)(void *, void *, VkDevice);
void VulkanDevice::deferredRelease(void *param0, void *param1, ReleaseFunc func, int num_frames) {
	DASSERT(num_frames <= max_defer_frames);
	m_impl->to_release[num_frames].emplace_back(param0, param1, func);
}

void VulkanDevice::nextReleasePhase() {
	auto &to_release = m_impl->to_release;
	for(auto [param0, param1, func] : to_release[0])
		func(param0, param1, m_handle);
	to_release[0].clear();
	for(int i = 1; i <= max_defer_frames; i++)
		to_release[i - 1].swap(to_release[i]);
}

template <class TObject> Pair<void *, VObjectId> VulkanDevice::allocObject() {
	auto type_id = VulkanTypeInfo<TObject>::type_id;

	auto &pools = m_impl->pools[type_id];
	auto &pool_list = m_impl->fillable_pools[type_id];
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
	int object_id = local_idx + (pool_idx << pool_size_log2);
	auto *pool_data = reinterpret_cast<PoolData<TObject> *>(pool.data);
	return {&pool_data->objects[local_idx], VObjectId(m_id, object_id)};
}

template <class TObject> void VulkanDevice::destroyObject(VulkanObjectBase<TObject> *ptr) {
	auto type_id = VulkanTypeInfo<TObject>::type_id;
	uint object_idx = uint(ptr->objectId().objectIdx());

	reinterpret_cast<TObject *>(ptr)->~TObject();

	uint local_idx = object_idx & ((1 << pool_size_log2) - 1);
	uint pool_idx = object_idx >> pool_size_log2;
	auto &pool = m_impl->pools[type_id][pool_idx];
	if(pool.usage_bits == ~0u)
		m_impl->fillable_pools[type_id].emplace_back(pool_idx);
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