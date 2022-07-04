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

class VulkanDeviceMemory : public VulkanObjectBase<VulkanDeviceMemory> {
  public:
	VMemoryFlags flags() const { return m_flags; }
	auto size() const { return m_size; }
	void dontDefer() { m_deferred_free = false; }

  private:
	friend class VulkanDevice;
	VulkanDeviceMemory(VkDeviceMemory, VObjectId, u64 size, VMemoryFlags);
	~VulkanDeviceMemory();

	u64 m_size;
	VMemoryFlags m_flags;
	bool m_deferred_free = true;
};

class VulkanDevice {
  public:
	Ex<PVSemaphore> createSemaphore(bool is_signaled = false);
	Ex<PVFence> createFence(bool is_signaled = false);
	Ex<PVShaderModule> createShaderModule(CSpan<char> bytecode);
	Ex<PVCommandPool> createCommandPool(VQueueFamilyId, VCommandPoolFlags);
	Ex<PVDeviceMemory> allocDeviceMemory(u64 size, uint memory_type_index);
	Ex<PVDeviceMemory> allocDeviceMemory(u64 size, u32 memory_type_bits, VMemoryFlags);
	Ex<PVSampler> createSampler(const VSamplingParams &);
	Ex<PVDescriptorPool> createDescriptorPool(const DescriptorPoolSetup &);

	Ex<void> createRenderGraph(PVSwapChain);

	VDeviceRef ref() const { return VDeviceRef(m_id); }
	VDeviceId id() const { return m_id; }
	VPhysicalDeviceId physId() const { return m_phys_id; }
	VkDevice handle() const { return m_handle; }
	VkPhysicalDevice physHandle() const { return m_phys_handle; }

	CSpan<Pair<VkQueue, VQueueFamilyId>> queues() const;

	struct MemoryDomainInfo {
		int type_index = -1;
		int heap_index = -1;
		u64 heap_size = 0;
	};

	VDeviceFeatures features() const { return m_features; }
	bool isAvailable(VMemoryDomain domain) const { return m_mem_domains[domain].type_index != -1; }
	const MemoryDomainInfo &info(VMemoryDomain domain) const { return m_mem_domains[domain]; }

	struct MemoryBudget {
		i64 heap_budget = -1;
		i64 heap_usage = -1;
	};

	// Will return -1 if budget extension is not available
	EnumMap<VMemoryDomain, MemoryBudget> memoryBudget() const;

	template <class THandle, class... Args>
	VPtr<THandle> createObject(THandle handle, Args &&...args) {
		using Object = typename VulkanHandleInfo<THandle>::Type;
		auto [ptr, object_id] = allocObject<Object>();
		auto *object = new(ptr) Object(handle, object_id, std::forward<Args>(args)...);
		return {object};
	}

	static constexpr int max_defer_frames = 3;
	using ReleaseFunc = void (*)(void *param0, void *param1, VkDevice);
	void deferredRelease(void *param0, void *param1, ReleaseFunc,
						 int num_frames = max_defer_frames);
	void nextReleasePhase();

	const VulkanRenderGraph &renderGraph() const { return *m_render_graph; }
	VulkanRenderGraph &renderGraph() { return *m_render_graph; }

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

	struct Impl;
	Dynamic<Impl> m_impl;
	Dynamic<VulkanRenderGraph> m_render_graph;

	VDeviceFeatures m_features;
	EnumMap<VMemoryDomain, MemoryDomainInfo> m_mem_domains;
	VkDevice m_handle = nullptr;
	VkPhysicalDevice m_phys_handle = nullptr;
	VInstanceRef m_instance_ref;
	VPhysicalDeviceId m_phys_id;
	VDeviceId m_id;
};
}