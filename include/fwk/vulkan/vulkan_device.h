// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct DescriptorPoolSetup;

struct VulkanQueueSetup {
	VQueueFamilyId family_id;
	int count;
};

struct VulkanDeviceSetup {
	vector<string> extensions;
	vector<VulkanQueueSetup> queues;
	Dynamic<VkPhysicalDeviceFeatures> features;
};

class VulkanCommandBuffer : public VulkanObjectBase<VulkanCommandBuffer> {
  public:
  private:
	friend class VulkanDevice;
	VulkanCommandBuffer(VkCommandBuffer, VObjectId, PVCommandPool);
	~VulkanCommandBuffer();

	PVCommandPool m_pool;
};

class VulkanCommandPool : public VulkanObjectBase<VulkanCommandPool> {
  public:
	Ex<PVCommandBuffer> allocBuffer();

  private:
	friend class VulkanDevice;
	VulkanCommandPool(VkCommandPool, VObjectId);
	~VulkanCommandPool();
};

class VulkanFence : public VulkanObjectBase<VulkanFence> {
  public:
  private:
	friend class VulkanDevice;
	VulkanFence(VkFence, VObjectId);
	~VulkanFence();
};

class VulkanSemaphore : public VulkanObjectBase<VulkanSemaphore> {
  public:
  private:
	friend class VulkanDevice;
	VulkanSemaphore(VkSemaphore, VObjectId);
	~VulkanSemaphore();
};

class VulkanDevice {
  public:
	Ex<void> createRenderGraph(PVSwapChain);

	VDeviceRef ref() const { return VDeviceRef(m_id); }
	VDeviceId id() const { return m_id; }
	VPhysicalDeviceId physId() const { return m_phys_id; }
	VkDevice handle() const { return m_handle; }
	VkPhysicalDevice physHandle() const { return m_phys_handle; }

	CSpan<Pair<VkQueue, VQueueFamilyId>> queues() const { return m_queues; }
	VDeviceFeatures features() const { return m_features; }

	const VulkanRenderGraph &renderGraph() const { return *m_render_graph; }
	VulkanRenderGraph &renderGraph() { return *m_render_graph; }

	const VulkanMemoryManager &memory() const { return *m_memory; }
	VulkanMemoryManager &memory() { return *m_memory; }

	Ex<void> beginFrame();
	Ex<void> finishFrame();

	// -------------------------------------------------------------------------------------------
	// ----------  Object management  ------------------------------------------------------------

  public:
	Ex<PVSemaphore> createSemaphore(bool is_signaled = false);
	Ex<PVFence> createFence(bool is_signaled = false);
	Ex<PVShaderModule> createShaderModule(CSpan<char> bytecode);
	Ex<PVCommandPool> createCommandPool(VQueueFamilyId, VCommandPoolFlags);
	Ex<PVSampler> createSampler(const VSamplingParams &);
	Ex<PVDescriptorPool> createDescriptorPool(const DescriptorPoolSetup &);

	template <class THandle, class... Args>
	VPtr<THandle> createObject(THandle handle, Args &&...args) {
		using Object = typename VulkanHandleInfo<THandle>::Type;
		auto [ptr, object_id] = allocObject<Object>();
		auto *object = new(ptr) Object(handle, object_id, std::forward<Args>(args)...);
		return {object};
	}

	// TODO: just use max_swap_frames from RenderGraph (or move it here)
	static constexpr int max_defer_frames = 3;
	using ReleaseFunc = void (*)(void *param0, void *param1, VkDevice);
	void deferredRelease(void *param0, void *param1, ReleaseFunc,
						 int num_frames = max_defer_frames);
	void nextReleasePhase();

	Ex<VMemoryBlock> alloc(VMemoryUsage, const VkMemoryRequirements &);

  private:
	VulkanDevice(VDeviceId, VPhysicalDeviceId, VInstanceRef);
	~VulkanDevice();

	VulkanDevice(const VulkanDevice &) = delete;
	void operator=(const VulkanDevice &) = delete;

	Ex<void> initialize(const VulkanDeviceSetup &);

	friend class VulkanInstance;
	friend class VulkanStorage;
	template <class T> friend class VulkanObjectBase;

	template <class TObject> Pair<void *, VObjectId> allocObject();
	template <class TObject> void destroyObject(VulkanObjectBase<TObject> *);
	struct ObjectPools;

	Dynamic<ObjectPools> m_objects;
	Dynamic<VulkanRenderGraph> m_render_graph;
	Dynamic<VulkanMemoryManager> m_memory;

	VDeviceFeatures m_features;
	// TODO: separate queue for transfers in the background?
	vector<Pair<VkQueue, VQueueFamilyId>> m_queues;
	VkDevice m_handle = nullptr;
	VkPhysicalDevice m_phys_handle = nullptr;
	VInstanceRef m_instance_ref;
	VPhysicalDeviceId m_phys_id;
	VDeviceId m_id;
};
}