// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VQueueSetup {
	VQueueFamilyId family_id;
	int count;
};

struct VDeviceSetup {
	vector<string> extensions;
	vector<VQueueSetup> queues;
	Dynamic<VkPhysicalDeviceFeatures> features;
};

class VulkanDevice {
  public:
	VDeviceRef ref() const { return VDeviceRef(m_id); }
	VDeviceId id() const { return m_id; }
	VPhysicalDeviceId physId() const { return m_phys_id; }
	VkDevice handle() const { return m_handle; }
	VkPhysicalDevice physHandle() const { return m_phys_handle; }

	CSpan<VQueue> queues() const { return m_queues; }
	Maybe<VQueue> findFirstQueue(VQueueCaps) const;

	VDeviceFeatures features() const { return m_features; }

	void addSwapChain(PVSwapChain);
	void removeSwapChain();
	PVSwapChain swapChain() const { return m_swap_chain; }

	Ex<void> createRenderGraph();
	const VulkanRenderGraph &renderGraph() const { return *m_render_graph; }
	VulkanRenderGraph &renderGraph() { return *m_render_graph; }

	const VulkanMemoryManager &memory() const { return *m_memory; }
	VulkanMemoryManager &memory() { return *m_memory; }

	// Returns result of SwapChain::acquireImage() or true
	Ex<bool> beginFrame();

	// Returns result of SwapChain::presentImage() or true
	Ex<bool> finishFrame();

	void waitForIdle();

	VkPipelineCache pipelineCache();

	// -------------------------------------------------------------------------------------------
	// ----------  Object management  ------------------------------------------------------------

  public:
	PVRenderPass getRenderPass(CSpan<VColorAttachment>, Maybe<VDepthAttachment> = none);
	PVPipelineLayout getPipelineLayout(CSpan<VDSLId>);
	PVSampler createSampler(const VSamplerSetup &);

	VDSLId getDSL(CSpan<DescriptorBindingInfo>);
	CSpan<DescriptorBindingInfo> bindings(VDSLId);
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
	void releaseObjects(int swap_frame_index);
	struct ObjectPools;

	Dynamic<VulkanDescriptorManager> m_descriptors;
	Dynamic<ObjectPools> m_objects;
	PVSwapChain m_swap_chain;
	Dynamic<VulkanRenderGraph> m_render_graph;
	Dynamic<VulkanMemoryManager> m_memory;

	VDeviceFeatures m_features;
	vector<VQueue> m_queues;
	VkDevice m_handle = nullptr;
	VkPhysicalDevice m_phys_handle = nullptr;
	VInstanceRef m_instance_ref;
	VPhysicalDeviceId m_phys_id;
	VDeviceId m_id;
	int m_swap_frame_index = 0;
};
}