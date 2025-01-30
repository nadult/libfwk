// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_device.h"

#include "fwk/gfx/image.h"
#include "fwk/vulkan/vulkan_command_queue.h"
#include "fwk/vulkan/vulkan_descriptor_manager.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_memory_manager.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "fwk/vulkan/vulkan_storage.h"

#include "fwk/format.h"
#include "fwk/hash_map.h"

#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_framebuffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_ray_tracing.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "fwk/vulkan/vulkan_swap_chain.h"
#include "fwk/vulkan/vulkan_window.h"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

// TODO: add option to hook an error handler which in case of (for example) out of memory error
// it can try to reclaim some memory by clearing the caches
// TODO: more concise form of descriptor declarations
// TODO: remove unnecessary layers (at the end)
// TODO: hide lower level functions somehow ?
// TODO: deferred releases are a bit messy
// TODO: per-frame buffers, images & image_views
// TODO: different interface for hashed objects? or maybe just hide create function and use weak refs?
// TODO: add check that objects which use frame allocator are actually destroyed in given frame
// TODO: add weak DeviceRef for passing into functions ?
// Maybe instead create functions should all be in device?
// TODO: not everything is released in waitForIdle (descriptors for example)
// TODO: add status function which prints number of objects of each type which are present
// TODO: another status function would be useful for memory
// TODO: don't store ref-counters in CMD lists in rendergraph (upload queue may be an exception)
// TODO(opt): upload command with multiple regions

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
	HashedPipelineLayout(CSpan<VDSLId> dsls, const VPushConstantRanges &pcrs, Maybe<u32> hash_value)
		: dsls(dsls), pcrs(pcrs),
		  hash_value(hash_value ? *hash_value : fwk::hashMany<u32>(dsls, pcrs.ranges)) {}

	u32 hash() const { return hash_value; }
	bool operator==(const HashedPipelineLayout &rhs) const {
		return hash_value == rhs.hash_value && dsls == rhs.dsls && pcrs.ranges == rhs.pcrs.ranges;
	}

	CSpan<VDSLId> dsls;
	const VPushConstantRanges &pcrs;
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
	HashMap<VSamplerSetup, PVSampler> hashed_samplers;

	EnumMap<VTypeId, vector<Pool>> pools;
	EnumMap<VTypeId, vector<u32>> fillable_pools;
	array<vector<DeferredRelease>, VulkanLimits::num_swap_frames> to_release;
};

struct VulkanDevice::DummyObjects {
	PVImageView dummy_image_2d;
	PVBuffer dummy_buffer;
};

// -------------------------------------------------------------------------------------------
// ---  Construction / destruction & main functions  -----------------------------------------

VulkanDevice::VulkanDevice(VDeviceId id, VPhysicalDeviceId phys_id, VInstanceRef instance_ref)
	: m_phys_id(phys_id), m_instance_ref(instance_ref), m_id(id) {
	m_objects.emplace();
	m_dummies.emplace();
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
	auto version = m_instance_ref->version();
	auto exts = setup.extensions;
	exts.emplace_back("VK_KHR_swapchain");
	if(version < VulkanVersion{1, 2, 0})
		exts.emplace_back("VK_KHR_separate_depth_stencil_layouts");

	if(anyOf(phys_info.extensions, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
		m_features |= VDeviceFeature::memory_budget;
		exts.emplace_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
		if(version < VulkanVersion{1, 1, 0})
			exts.emplace_back("VK_KHR_get_physical_device_properties2");
	}

	if(version >= VulkanVersion{1, 3, 0})
		m_features |= VDeviceFeature::subgroup_size_control;
	else {
		if(anyOf(phys_info.extensions, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME) &&
		   phys_info.subgroup_control_props.requiredSubgroupSizeStages != 0) {
			m_features |= VDeviceFeature::subgroup_size_control;
			exts.emplace_back(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
		}
	}

	if(anyOf(phys_info.extensions, VK_KHR_SHADER_CLOCK_EXTENSION_NAME)) {
		exts.emplace_back(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);
		m_features |= VDeviceFeature::shader_clock;
	}

	vector<string> raytracing_exts{
		{VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		 VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME}};
	if(allOf(raytracing_exts,
			 [&](const string &ext) { return anyOf(phys_info.extensions, ext); })) {
		insertBack(exts, raytracing_exts);
		m_features |= VDeviceFeature::ray_tracing;
	}

	makeSortedUnique(exts);

	for(auto ext : exts)
		if(!isOneOf(ext, phys_info.extensions))
			return ERROR("Mandatory extension not supported: '%'", ext);

	VkPhysicalDeviceFeatures features{};
	if(setup.features)
		features = *setup.features;

	features.fillModeNonSolid = VK_TRUE;
	features.samplerAnisotropy = VK_TRUE;

	VkPhysicalDeviceVulkan12Features v12_features{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	if(m_features & VDeviceFeature::ray_tracing)
		v12_features.bufferDeviceAddress = VK_TRUE;
	v12_features.separateDepthStencilLayouts = VK_TRUE;

	VkPhysicalDeviceShaderClockFeaturesKHR clock_features{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR};
	clock_features.shaderDeviceClock = VK_TRUE;
	clock_features.shaderSubgroupClock = VK_TRUE;
	VkPhysicalDeviceSubgroupSizeControlFeatures subgroup_features{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES};
	if(m_features & VDeviceFeature::subgroup_size_control) {
		subgroup_features.subgroupSizeControl = VK_TRUE;
		clock_features.pNext = &subgroup_features;
		v12_features.pNext = &clock_features;
	}

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_features{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
	VkPhysicalDeviceRayQueryFeaturesKHR rq_features{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR acc_features{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
	if(m_features & VDeviceFeature::ray_tracing) {
		rt_features.pNext = &rq_features;
		rq_features.pNext = &acc_features;
		acc_features.pNext = v12_features.pNext;
		v12_features.pNext = &rt_features;
		rt_features.rayTracingPipeline = VK_TRUE;
		rq_features.rayQuery = VK_TRUE;
		acc_features.accelerationStructure = VK_TRUE;
	}

	vector<const char *> c_exts = transform(exts, [](auto &str) { return str.c_str(); });
	VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	ci.pQueueCreateInfos = queue_cis.data();
	ci.queueCreateInfoCount = queue_cis.size();
	ci.pEnabledFeatures = &features;
	ci.enabledExtensionCount = c_exts.size();
	ci.ppEnabledExtensionNames = c_exts.data();
	ci.pNext = &v12_features;

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
	auto mem_setup = setup.memory;
	mem_setup.enable_device_address |= !!(m_features & VDeviceFeature::ray_tracing);
	m_memory.emplace(m_handle, m_instance_ref->info(m_phys_id), m_features, mem_setup);

	m_cmds = new VulkanCommandQueue(*this);
	EXPECT(m_cmds->initialize(ref()));

	VImageSetup img_setup{VFormat::rgba8_unorm, int2(4, 4)};
	auto white_image = EX_PASS(VulkanImage::create(*this, img_setup));
	EXPECT(white_image->upload(Image({4, 4}, ColorId::white)));
	m_dummies->dummy_image_2d = VulkanImageView::create(white_image);

	auto rt_usage =
		VBufferUsage::accel_struct_storage | VBufferUsage::accel_struct_build_input_read_only;
	auto dummy_usage = (VBufferUsageFlags(all<VBufferUsage>) & ~rt_usage) |
					   mask(m_features & VDeviceFeature::ray_tracing, rt_usage);
	m_dummies->dummy_buffer =
		EX_PASS(VulkanBuffer::create(*this, 1024, dummy_usage, VMemoryUsage::device));

	return {};
}

VulkanDevice::~VulkanDevice() {
	m_pipeline_caches.clear();
	m_dummies.reset();
	if(m_handle) {
		vkDestroyPipelineCache(m_handle, m_objects->pipeline_cache, nullptr);
		vkDeviceWaitIdle(m_handle);
	}
	m_swap_chain = {};
	m_descriptors.reset();
	m_cmds.reset();
	m_objects->hashed_render_passes.clear();
	m_objects->hashed_framebuffers.clear();
	m_objects->hashed_pipeline_layouts.clear();
	m_objects->hashed_samplers.clear();

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
		FWK_FATAL("All VPtrs<> to Vulkan objects should be freed before VulkanDevice is destroyed");
#endif
	m_memory.reset();

	if(m_handle) {
		for(int i : intRange(VulkanLimits::num_swap_frames))
			releaseObjects(i);
		g_vk_storage.device_handles[m_id] = nullptr;
		vkDestroyDevice(m_handle, nullptr);
	}
}

const VulkanPhysicalDeviceInfo &VulkanDevice::physInfo() const {
	return VulkanInstance::ref()->info(m_phys_id);
}

Maybe<VQueue> VulkanDevice::findFirstQueue(VQueueCaps caps) const {
	for(auto &queue : m_queues)
		if((queue.caps & caps) == caps)
			return queue;
	return none;
}

Ex<void> VulkanDevice::addSwapChain(VWindowRef window, VSwapChainSetup setup) {
	auto swap_chain = EX_PASS(VulkanSwapChain::create(*this, window, setup));
	addSwapChain(swap_chain);
	return {};
}

void VulkanDevice::addSwapChain(PVSwapChain swap_chain) {
	DASSERT("Currently only single swap chain can be used by VulkanDevice" && !m_swap_chain);
	if(m_cmds)
		DASSERT(m_cmds->status() != VulkanCommandQueue::Status::frame_running);
	m_swap_chain = swap_chain;
}

void VulkanDevice::removeSwapChain() {
	DASSERT(m_swap_chain);
	DASSERT(m_swap_chain->status() != VSwapChainStatus::image_acquired);
	m_swap_chain = {};
}

Ex<> VulkanDevice::beginFrame() {
	DASSERT(m_cmds);
	if(m_swap_chain)
		m_image_available_sem = EX_PASS(m_swap_chain->acquireImage());
	m_cmds->waitForFrameFence();
	m_swap_frame_index = m_cmds->swapFrameIndex();
	releaseObjects(m_swap_frame_index);
	m_cmds->beginFrame();
	m_memory->beginFrame();
	m_descriptors->beginFrame(m_swap_frame_index);
	return {};
}

Ex<> VulkanDevice::finishFrame() {
	DASSERT(m_cmds);
	m_descriptors->finishFrame();
	m_memory->finishFrame();

	if(m_swap_chain && m_image_available_sem) {
		VkSemaphore render_finished_sem = nullptr;
		m_cmds->finishFrame(&m_image_available_sem, &render_finished_sem);
		EXPECT(m_swap_chain->presentImage(render_finished_sem));
		m_image_available_sem = nullptr;
	} else {
		m_cmds->finishFrame(nullptr, nullptr);
	}
	cleanupFramebuffers();
	return {};
}

void VulkanDevice::waitForIdle() {
	DASSERT(m_cmds->status() != VulkanCommandQueue::Status::frame_running);
	vkDeviceWaitIdle(m_handle);
	for(int i : intRange(VulkanLimits::num_swap_frames))
		releaseObjects(i);
	m_memory->flushDeferredFrees();
}

VkPipelineCache VulkanDevice::pipelineCache() { return m_objects->pipeline_cache; }

VulkanVersion VulkanDevice::version() const { return m_instance_ref->version(); }

VDepthStencilFormat VulkanDevice::bestSupportedFormat(VDepthStencilFormat format) const {
	auto supported = physInfo().supported_depth_stencil_formats;
	DASSERT(supported != none);
	if(format & supported)
		return format;

	uint min_depth_bits = depthBits(format);
	bool req_stencil = hasStencil(format);

	Maybe<VDepthStencilFormat> best;
	uint best_bits = 666;
	for(auto format : supported) {
		if(req_stencil && !hasStencil(format))
			continue;
		uint depth_bits = depthBits(format);
		if(depth_bits >= min_depth_bits && depth_bits < best_bits) {
			best = format;
			best_bits = depth_bits;
		}
	}

	return *best;
}

// -------------------------------------------------------------------------------------------
// ----------  Object management  ------------------------------------------------------------

PVRenderPass VulkanDevice::getRenderPass(CSpan<VColorAttachment> colors,
										 Maybe<VDepthAttachment> depth) {
	HashedRenderPass key(colors, depth ? &*depth : nullptr, none);
	auto &hash_map = m_objects->hashed_render_passes;
	auto it = hash_map.find(key);
	if(it != hash_map.end())
		return it->value;

	auto pointer = VulkanRenderPass::create(*this, colors, depth);
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
		color_atts.emplace_back(color->format(), color->dimensions().num_samples);
	Maybe<VDepthAttachment> depth_att;
	if(depth) {
		auto ds_format = fromVkDepthStencilFormat(depth->format());
		DASSERT(ds_format);
		depth_att = VDepthAttachment(*ds_format, depth->dimensions().num_samples);
	}
	auto render_pass = getRenderPass(color_atts, depth_att);

	auto pointer = VulkanFramebuffer::create(render_pass, colors, depth);
	hash_map.emplace(HashedFramebuffer(pointer->colors(), depth, key.hash_value), pair{pointer, 0});
	return pointer;
}

PVPipelineLayout VulkanDevice::getPipelineLayout(CSpan<VDSLId> dsls,
												 const VPushConstantRanges &pcrs) {
	HashedPipelineLayout key(dsls, pcrs, none);
	auto &hash_map = m_objects->hashed_pipeline_layouts;
	auto it = hash_map.find(key);
	if(it != hash_map.end())
		return it->value;

	auto pointer = VulkanPipelineLayout::create(*this, dsls, pcrs);
	hash_map.emplace(HashedPipelineLayout(pointer->descriptorSetLayouts(),
										  pointer->pushConstantRanges(), key.hash_value),
					 pointer);
	return pointer;
}

PVPipelineLayout VulkanDevice::getPipelineLayout(CSpan<PVShaderModule> shader_modules,
												 const VPushConstantRanges &pcrs) {
	vector<VDescriptorBindingInfo> descr_bindings;
	for(auto &shader_module : shader_modules) {
		DASSERT(shader_module);
		auto stage_bindings = shader_module->descriptorBindingInfos();
		if(descr_bindings.empty())
			descr_bindings = stage_bindings;
		else
			descr_bindings = VDescriptorBindingInfo::merge(descr_bindings, stage_bindings);
	}

	auto descr_sets = VDescriptorBindingInfo::divideSets(descr_bindings);

	vector<VDSLId> dsls;
	dsls.reserve(descr_sets.size());
	int set_index = 0;
	for(auto bindings : descr_sets) {
		PASSERT(!bindings.empty());
		int bindings_set_index = bindings[0].set();
		DASSERT(bindings_set_index >= set_index);
		while(set_index < bindings_set_index) {
			dsls.emplace_back(m_descriptors->getEmptyLayout());
			set_index++;
		}
		dsls.emplace_back(getDSL(bindings));
		set_index++;
	}
	return getPipelineLayout(dsls, pcrs);
}

PVPipelineLayout VulkanDevice::getPipelineLayout(CSpan<PVShaderModule> shader_modules) {
	return getPipelineLayout(shader_modules, {});
}

VDSLId VulkanDevice::getDSL(CSpan<VDescriptorBindingInfo> bindings) {
	DASSERT(bindings);
	return m_descriptors->getLayout(bindings);
}

CSpan<VDescriptorBindingInfo> VulkanDevice::bindings(VDSLId dsl_id) {
	return m_descriptors->bindings(dsl_id);
}

VDescriptorSet VulkanDevice::acquireSet(VDSLId dsl_id) {
	auto handle = m_descriptors->acquireSet(dsl_id);
	return VDescriptorSet(*this, handle, m_descriptors->bindingMap(dsl_id));
}

VkDescriptorSetLayout VulkanDevice::handle(VDSLId dsl_id) { return m_descriptors->handle(dsl_id); }

PVSampler VulkanDevice::getSampler(const VSamplerSetup &setup) {
	auto &ref = m_objects->hashed_samplers[setup];
	if(!ref) {
		VkSamplerCreateInfo ci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		ci.magFilter = VkFilter(setup.mag_filter);
		ci.minFilter = VkFilter(setup.min_filter);
		ci.mipmapMode = VkSamplerMipmapMode(setup.mipmap_filter.orElse(VTexFilter::nearest));
		ci.addressModeU = VkSamplerAddressMode(setup.address_mode[0]);
		ci.addressModeV = VkSamplerAddressMode(setup.address_mode[1]);
		ci.addressModeW = VkSamplerAddressMode(setup.address_mode[2]);
		ci.maxAnisotropy = setup.max_anisotropy_samples;
		ci.anisotropyEnable = setup.max_anisotropy_samples > 1;
		ci.maxLod = setup.mipmap_filter ? VK_LOD_CLAMP_NONE : 0.0;
		VkSampler handle;
		FWK_VK_CALL(vkCreateSampler, m_handle, &ci, nullptr, &handle);
		ref = createObject(handle, setup);
	}
	return ref;
}

PVImageView VulkanDevice::dummyImage2D() const { return m_dummies->dummy_image_2d; }
PVBuffer VulkanDevice::dummyBuffer() const { return m_dummies->dummy_buffer; }

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

	// TODO: are we sure that this is faster than new & delete?
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
	DASSERT(pool.usage_bits != ~0u);
	int local_idx = countTrailingZeros(~pool.usage_bits);
	pool.usage_bits |= (1u << local_idx);
	if(pool.usage_bits == ~0u)
		pool_list.pop_back();
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
