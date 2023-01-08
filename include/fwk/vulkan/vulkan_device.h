// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/hash_map.h"
#include "fwk/light_tuple.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanDevice {
  public:
	VDeviceRef ref() const { return VDeviceRef(m_id); }
	VDeviceId id() const { return m_id; }
	VPhysicalDeviceId physId() const { return m_phys_id; }
	VkDevice handle() const { return m_handle; }
	VkPhysicalDevice physHandle() const { return m_phys_handle; }
	const VulkanPhysicalDeviceInfo &physInfo() const;

	CSpan<VQueue> queues() const { return m_queues; }
	Maybe<VQueue> findFirstQueue(VQueueCaps) const;

	VDeviceFeatures features() const { return m_features; }
	VulkanVersion version() const;

	Ex<void> addSwapChain(VWindowRef, VSwapChainSetup = {});
	void addSwapChain(PVSwapChain);
	void removeSwapChain();
	PVSwapChain swapChain() const { return m_swap_chain; }

	const VulkanCommandQueue &cmdQueue() const { return *m_cmds; }
	VulkanCommandQueue &cmdQueue() { return *m_cmds; }

	const VulkanMemoryManager &memory() const { return *m_memory; }
	VulkanMemoryManager &memory() { return *m_memory; }

	// Error might come from SwapChain::acquireImage()
	Ex<> beginFrame();
	// Error might come from SwapChain::presentImage()
	Ex<> finishFrame();

	void waitForIdle();

	VkPipelineCache pipelineCache();

	VDepthStencilFormat bestSupportedFormat(VDepthStencilFormat) const;

	// -------------------------------------------------------------------------------------------
	// ----------  Object management  ------------------------------------------------------------

  public:
	PVRenderPass getRenderPass(CSpan<VColorAttachment>, Maybe<VDepthAttachment> = none);
	PVFramebuffer getFramebuffer(CSpan<PVImageView> colors, PVImageView depth = none);
	PVPipelineLayout getPipelineLayout(CSpan<VDSLId>, const VPushConstantRanges &);
	PVPipelineLayout getPipelineLayout(CSpan<PVShaderModule>, const VPushConstantRanges &);
	PVPipelineLayout getPipelineLayout(CSpan<PVShaderModule>);

	PVSampler getSampler(const VSamplerSetup &);

	template <class... Args>
	using PipelineFunc = Ex<PVPipeline> (*)(ShaderCompiler &, VulkanDevice &, Args...);

	template <class... Args>
	Ex<PVPipeline> getCachedPipeline(ShaderCompiler &compiler, PipelineFunc<Decay<Args>...> func,
									 Args &&...args) {
		auto make_cache_func = [](void *func) -> PipelineCacheModel * {
			return new PipelineCache<Args...>(func);
		};
		auto *cache = reinterpret_cast<PipelineCache<Args...> *>(
			getPipelineCache((void *)func, make_cache_func));
		return cache->get(compiler, *this, std::forward<Args>(args)...);
	}

	PVImageView dummyImage2D() const;
	PVBuffer dummyBuffer() const;

	VDSLId getDSL(CSpan<VDescriptorBindingInfo>);
	CSpan<VDescriptorBindingInfo> bindings(VDSLId);
	VDescriptorSet acquireSet(VDSLId);
	VkDescriptorSetLayout handle(VDSLId);

	template <class THandle, class... Args>
	VPtr<THandle> createObject(THandle handle, Args &&...args) {
		using Object = typename VulkanHandleInfo<THandle>::Type;
		auto [ptr, object_id] = allocObject<Object>();
		auto *object = new(ptr) Object(handle, object_id, std::forward<Args>(args)...);
		return {object};
	}

	using ReleaseFunc = void (*)(void *param0, void *param1, VkDevice);
	void deferredRelease(void *param0, void *param1, ReleaseFunc);

	Ex<VMemoryBlock> alloc(VMemoryUsage, const VkMemoryRequirements &);
	void deferredFree(VMemoryBlockId);

  private:
	VulkanDevice(VDeviceId, VPhysicalDeviceId, VInstanceRef);
	~VulkanDevice();

	VulkanDevice(const VulkanDevice &) = delete;
	void operator=(const VulkanDevice &) = delete;

	Ex<void> initialize(const VDeviceSetup &);

	friend class VulkanInstance;
	friend class VulkanStorage;
	template <class T> friend class VulkanObjectBase;

	template <class TObject> Pair<void *, VObjectId> allocObject();
	template <class TObject> void destroyObject(VulkanObjectBase<TObject> *);

	void cleanupFramebuffers();
	void releaseObjects(int swap_frame_index);

	struct ObjectPools;
	struct DummyObjects;
	struct PipelineCaches;

	struct PipelineCacheModel {
		virtual ~PipelineCacheModel() = default;
		virtual PipelineCacheModel *clone() const = 0;
	};

	template <class... Args> struct PipelineCache final : public PipelineCacheModel {
		using Func = PipelineFunc<Decay<Args>...>;
		using Key = LightTuple<Decay<RemoveReference<Args>>...>;
		PipelineCache(void *func) : function(Func(func)) {}

		PipelineCacheModel *clone() const {
			auto out = new PipelineCache((void *)function);
			out->pipelines = pipelines;
			return out;
		}

		Ex<PVPipeline> get(ShaderCompiler &compiler, VulkanDevice &device, Args &&...args) {
			Key key{std::forward<Args>(args)...};
			auto it = pipelines.find(key);
			if(it != pipelines.end())
				return it->value;

			auto result = EX_PASS(function(compiler, device, std::forward<Args>(args)...));
			pipelines.emplace(key, result);
			return result;
		}

		Func function;
		HashMap<Key, PVPipeline> pipelines;
	};

	PipelineCacheModel *getPipelineCache(void *func, PipelineCacheModel *(*make_func)(void *)) {
		auto it = m_pipeline_caches.find(func);
		if(it != m_pipeline_caches.end())
			return it->value.get();

		auto *result = make_func(func);
		Dynamic<PipelineCacheModel> model(result);
		m_pipeline_caches.emplace(func, move(model));
		return result;
	}

	Dynamic<VulkanDescriptorManager> m_descriptors;
	Dynamic<ObjectPools> m_objects;
	Dynamic<DummyObjects> m_dummies;
	HashMap<void *, Dynamic<PipelineCacheModel>> m_pipeline_caches;

	PVSwapChain m_swap_chain;
	Dynamic<VulkanCommandQueue> m_cmds;
	Dynamic<VulkanMemoryManager> m_memory;

	VDeviceFeatures m_features;
	vector<VQueue> m_queues;
	VkDevice m_handle = nullptr;
	VkPhysicalDevice m_phys_handle = nullptr;
	VkSemaphore m_image_available_sem = nullptr;
	VInstanceRef m_instance_ref;
	VPhysicalDeviceId m_phys_id;
	VDeviceId m_id;
	int m_swap_frame_index = 0;
};
}