// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_device.h"

#include "fwk/vulkan/vulkan_descriptor_manager.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_memory_manager.h"
#include "fwk/vulkan/vulkan_render_graph.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "fwk/vulkan/vulkan_storage.h"

#include "fwk/format.h"
#include "fwk/hash_map.h"

#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_framebuffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "fwk/vulkan/vulkan_swap_chain.h"
#include "fwk/vulkan/vulkan_window.h"

// TODO: add option to hook an error handler which in case of (for example) out of memory error
// it can try to reclaim some memory by clearing the caches
// TODO: more concise form of descriptor declarations
// TODO: remove unnecessary layers (at the end)
// TODO: cleanup in queue interface
// TODO: hide lower level functions somehow ?

namespace fwk {

// -------------------------------------------------------------------------------------------
// ------  Implementation struct declarations  -----------------------------------------------

static constexpr int object_pool_size = 32;
static constexpr int object_pool_size_log2 = 5;

template <class T> struct PoolData {
	std::aligned_storage_t<sizeof(T), alignof(T)> objects[object_pool_size];
};

struct HashedRenderPass {
	HashedRenderPass(CSpan<VColorAttachment> colors, const VDepthAttachment *depth,
					 Maybe<u32> hash_value)
		: colors(colors), depth(depth),
		  hash_value(hash_value.orElse(VulkanRenderPass::hashConfig(colors, depth))) {}

	u32 hash() const { return hash_value; }
	bool operator==(const HashedRenderPass &rhs) const {
		return hash_value == rhs.hash_value && colors == rhs.colors &&
			   bool(depth) == bool(rhs.depth) && (!depth || *depth == *rhs.depth);
	}

	CSpan<VColorAttachment> colors;
	const VDepthAttachment *depth;
	u32 hash_value;
};

struct HashedFramebuffer {
	HashedFramebuffer(CSpan<PVImageView> colors, PVImageView depth, Maybe<u32> hash_value)
		: colors(colors), depth(depth),
		  hash_value(hash_value.orElse(VulkanFramebuffer::hashConfig(colors, depth))) {}

	u32 hash() const { return hash_value; }
	bool operator==(const HashedFramebuffer &rhs) const {
		return hash_value == rhs.hash_value && colors == rhs.colors && depth == rhs.depth;
	}

	CSpan<PVImageView> colors;
	PVImageView depth;
	u32 hash_value;
};

struct HashedPipelineLayout {
	HashedPipelineLayout(CSpan<VDSLId> dsls, Maybe<u32> hash_value)
		: dsls(dsls), hash_value(hash_value.orElse(fwk::hash<u32>(dsls))) {}

	u32 hash() const { return hash_value; }
	bool operator==(const HashedPipelineLayout &rhs) const {
		return hash_value == rhs.hash_value && dsls == rhs.dsls;
	}

	CSpan<VDSLId> dsls;
	u32 hash_value;
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

	VkPipelineCache pipeline_cache = nullptr;

	HashMap<HashedRenderPass, PVRenderPass> hashed_render_passes;
	HashMap<HashedFramebuffer, Pair<PVFramebuffer, int>> hashed_framebuffers;
	HashMap<HashedPipelineLayout, PVPipelineLayout> hashed_pipeline_layouts;

	EnumMap<VTypeId, vector<Pool>> pools;
	EnumMap<VTypeId, vector<u32>> fillable_pools;
	array<vector<DeferredRelease>, VulkanLimits::num_swap_frames> to_release;
};

// -------------------------------------------------------------------------------------------
// ---  Construction / destruction & main functions  -----------------------------------------

VulkanDevice::VulkanDevice(VDeviceId id, VPhysicalDeviceId phys_id, VInstanceRef instance_ref)
	: m_id(id), m_phys_id(phys_id), m_instance_ref(instance_ref) {
	m_objects.emplace();
	ASSERT(m_instance_ref->valid(m_phys_id));
}

Ex<void> VulkanDevice::initialize(const VDeviceSetup &setup) {
	EXPECT(!setup.queues.empty());

	vector<VkDeviceQueueCreateInfo> queue_cis;
	queue_cis.reserve(setup.queues.size());

	float default_priority = 1.0;
	for(auto &queue : setup.queues) {
		VkDeviceQueueCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
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
	VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	ci.pQueueCreateInfos = queue_cis.data();
	ci.queueCreateInfoCount = queue_cis.size();
	ci.pEnabledFeatures = &features;
	ci.enabledExtensionCount = c_exts.size();
	ci.ppEnabledExtensionNames = c_exts.data();

	m_phys_handle = phys_info.handle;
	FWK_VK_EXPECT_CALL(vkCreateDevice, m_phys_handle, &ci, nullptr, &m_handle);
	g_vk_storage.device_handles[m_id] = m_handle;

	m_queues.reserve(setup.queues.size());
	for(auto &def : setup.queues) {
		for(int i : intRange(def.count)) {
			VkQueue queue = nullptr;
			vkGetDeviceQueue(m_handle, def.family_id, i, &queue);
			uint vk_flags = phys_info.queue_families[def.family_id].queueFlags;
			VQueueCaps caps(vk_flags & VQueueCaps::mask);
			m_queues.emplace_back(queue, def.family_id, caps);
		}
	}

	VkPipelineCacheCreateInfo pip_ci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
	//pip_ci.flags = VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
	FWK_VK_EXPECT_CALL(vkCreatePipelineCache, m_handle, &pip_ci, nullptr,
					   &m_objects->pipeline_cache);

	m_descriptors.emplace(m_handle);
	m_memory.emplace(m_handle, m_instance_ref->info(m_phys_id), m_features);
	return {};
}

VulkanDevice::~VulkanDevice() {
	if(m_handle) {
		vkDestroyPipelineCache(m_handle, m_objects->pipeline_cache, nullptr);
		vkDeviceWaitIdle(m_handle);
	}
	m_swap_chain = {};
	m_descriptors.reset();
	m_render_graph.reset();
	m_objects->hashed_render_passes.clear();
	m_objects->hashed_framebuffers.clear();
	m_objects->hashed_pipeline_layouts.clear();

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
		for(int i : intRange(VulkanLimits::num_swap_frames))
			releaseObjects(i);
		g_vk_storage.device_handles[m_id] = nullptr;
		vkDestroyDevice(m_handle, nullptr);
	}
}

Maybe<VQueue> VulkanDevice::findFirstQueue(VQueueCaps caps) const {
	for(auto &queue : m_queues)
		if((queue.caps & caps) == caps)
			return queue;
	return none;
}

void VulkanDevice::addSwapChain(PVSwapChain swap_chain) {
	DASSERT("Currently only single swap chain can be used by VulkanDevice" && !m_swap_chain);
	if(m_render_graph)
		DASSERT(m_render_graph->status() != VulkanRenderGraph::Status::frame_running);
	m_swap_chain = swap_chain;
}

void VulkanDevice::removeSwapChain() {
	DASSERT(m_swap_chain);
	DASSERT(m_swap_chain->status() != VSwapChainStatus::image_acquired);
	m_swap_chain = {};
}

Ex<void> VulkanDevice::createRenderGraph() {
	DASSERT(!m_render_graph && "Render graph already initialized");
	m_render_graph = new VulkanRenderGraph(ref());
	return m_render_graph->initialize(ref());
}

Ex<bool> VulkanDevice::beginFrame() {
	DASSERT(m_render_graph);
	if(m_swap_chain) {
		m_image_available_sem = EX_PASS(m_swap_chain->acquireImage());
		if(!m_image_available_sem)
			return false;
	}
	m_render_graph->beginFrame();
	m_swap_frame_index = m_render_graph->swapFrameIndex();
	m_memory->beginFrame();
	m_descriptors->beginFrame(m_swap_frame_index);
	releaseObjects(m_swap_frame_index);
	return true;
}

Ex<bool> VulkanDevice::finishFrame() {
	DASSERT(m_render_graph);
	m_descriptors->finishFrame();
	m_memory->finishFrame();

	if(m_swap_chain) {
		auto render_finished_sem = m_render_graph->finishFrame(cspan(&m_image_available_sem, 1));
		EXPECT(m_swap_chain->presentImage(cspan(&render_finished_sem, 1)));
	} else {
		m_render_graph->finishFrame({});
	}

	cleanupFramebuffers();

	return true;
}

void VulkanDevice::waitForIdle() {
	DASSERT(m_render_graph->status() != VulkanRenderGraph::Status::frame_running);
	vkDeviceWaitIdle(m_handle);
	for(int i : intRange(VulkanLimits::num_swap_frames)) {
		releaseObjects(i);
		m_memory->freeDeferred(i);
	}
}

VkPipelineCache VulkanDevice::pipelineCache() { return m_objects->pipeline_cache; }

// -------------------------------------------------------------------------------------------
// ----------  Object management  ------------------------------------------------------------

PVRenderPass VulkanDevice::getRenderPass(CSpan<VColorAttachment> colors,
										 Maybe<VDepthAttachment> depth) {
	HashedRenderPass key(colors, depth ? &*depth : nullptr, none);
	auto &hash_map = m_objects->hashed_render_passes;
	auto it = hash_map.find(key);
	if(it != hash_map.end())
		return it->value;

	auto pointer = VulkanRenderPass::create(ref(), colors, depth);
	auto depth_ptr = pointer->depth() ? &*pointer->depth() : nullptr;
	hash_map.emplace(HashedRenderPass(pointer->colors(), depth_ptr, key.hash_value), pointer);
	return pointer;
}

PVFramebuffer VulkanDevice::getFramebuffer(CSpan<PVImageView> colors, PVImageView depth) {
	HashedFramebuffer key(colors, depth, none);
	auto &hash_map = m_objects->hashed_framebuffers;
	auto it = hash_map.find(key);
	if(it != hash_map.end()) {
		it->value.second = 0;
		return it->value.first;
	}

	StaticVector<VColorAttachment, VulkanLimits::max_color_attachments> color_atts;
	for(auto color : colors)
		color_atts.emplace_back(color->format(), color->numSamples());
	Maybe<VDepthAttachment> depth_att;
	if(depth)
		depth_att = VDepthAttachment(depth->format(), depth->numSamples());
	auto render_pass = getRenderPass(color_atts, depth_att);

	auto pointer = VulkanFramebuffer::create(ref(), render_pass, colors, depth);
	hash_map.emplace(HashedFramebuffer(pointer->colors(), depth, key.hash_value), pair{pointer, 0});
	return pointer;
}

PVPipelineLayout VulkanDevice::getPipelineLayout(CSpan<VDSLId> dsls) {
	HashedPipelineLayout key(dsls, none);
	auto &hash_map = m_objects->hashed_pipeline_layouts;
	auto it = hash_map.find(key);
	if(it != hash_map.end())
		return it->value;

	auto pointer = VulkanPipelineLayout::create(ref(), dsls);
	hash_map.emplace(HashedPipelineLayout(pointer->descriptorSetLayouts(), key.hash_value),
					 pointer);
	return pointer;
}

VDSLId VulkanDevice::getDSL(CSpan<DescriptorBindingInfo> bindings) {
	return m_descriptors->getLayout(bindings);
}

CSpan<DescriptorBindingInfo> VulkanDevice::bindings(VDSLId dsl_id) {
	return m_descriptors->bindings(dsl_id);
}

VDescriptorSet VulkanDevice::acquireSet(VDSLId dsl_id) {
	auto handle = m_descriptors->acquireSet(dsl_id);
	return VDescriptorSet(m_handle, handle);
}

VkDescriptorSetLayout VulkanDevice::handle(VDSLId dsl_id) { return m_descriptors->handle(dsl_id); }

PVSampler VulkanDevice::createSampler(const VSamplerSetup &params) {
	VkSamplerCreateInfo ci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	ci.magFilter = VkFilter(params.mag_filter);
	ci.minFilter = VkFilter(params.min_filter);
	ci.mipmapMode = VkSamplerMipmapMode(params.mipmap_filter.orElse(VTexFilter::nearest));
	ci.addressModeU = VkSamplerAddressMode(params.address_mode[0]);
	ci.addressModeV = VkSamplerAddressMode(params.address_mode[1]);
	ci.addressModeW = VkSamplerAddressMode(params.address_mode[2]);
	ci.maxAnisotropy = params.max_anisotropy_samples;
	ci.anisotropyEnable = params.max_anisotropy_samples > 1;
	VkSampler handle;
	FWK_VK_CALL(vkCreateSampler, m_handle, &ci, nullptr, &handle);
	return createObject(handle, params);
}

using ReleaseFunc = void (*)(void *, void *, VkDevice);
void VulkanDevice::deferredRelease(void *param0, void *param1, ReleaseFunc func) {
	m_objects->to_release[m_swap_frame_index].emplace_back(param0, param1, func);
}

// We're removing framebuffers which weren't used for some time, or those which
// keep refs to images unreferenced anywhere else
void VulkanDevice::cleanupFramebuffers() {
	static constexpr int cleanup_num_frames = 16;

	auto &hash_map = m_objects->hashed_framebuffers;
	for(auto it = hash_map.begin(); it != hash_map.end(); ++it) {
		auto &ref = *it->value.first;
		if(ref.refCount() == 1) {
			auto &unused_counter = it->value.second;
			unused_counter++;

			bool needs_cleanup = unused_counter >= cleanup_num_frames;
			if(!needs_cleanup) {
				bool last_refs = !ref.hasDepth() || ref.depth()->refCount() == 1;
				for(auto &color : ref.colors())
					last_refs &= color->refCount() == 1;
				needs_cleanup = last_refs;
			}
			if(needs_cleanup)
				hash_map.erase(it);
		}
	}
}

void VulkanDevice::releaseObjects(int frame_index) {
	auto &to_release = m_objects->to_release[frame_index];
	for(auto [param0, param1, func] : to_release)
		func(param0, param1, m_handle);
	to_release.clear();
}

Ex<VMemoryBlock> VulkanDevice::alloc(VMemoryUsage usage, const VkMemoryRequirements &requirements) {
	return m_memory->alloc(usage, requirements);
}

void VulkanDevice::deferredFree(VMemoryBlockId id) { m_memory->deferredFree(id); }

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
void VulkanObjectBase<T>::deferredRelease(void *p0, void *p1, ReleaseFunc release_func) {
	auto &device = reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[deviceId()]);
	device.deferredRelease(p0, p1, release_func);
}

template <class T> void VulkanObjectBase<T>::deferredFree(VMemoryBlockId block_id) {
	auto &device = reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[deviceId()]);
	device.deferredFree(block_id);
}

#define CASE_TYPE(UpperCase, VkName, _)                                                            \
	template Pair<void *, VObjectId> VulkanDevice::allocObject<Vulkan##UpperCase>();               \
	template void VulkanDevice::destroyObject(VulkanObjectBase<Vulkan##UpperCase> *);              \
	template void VulkanObjectBase<Vulkan##UpperCase>::destroyObject();                            \
	template void VulkanObjectBase<Vulkan##UpperCase>::deferredRelease(void *, void *,             \
																	   ReleaseFunc);               \
	template void VulkanObjectBase<Vulkan##UpperCase>::deferredFree(VMemoryBlockId);
#include "fwk/vulkan/vulkan_type_list.h"
}