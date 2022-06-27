// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VulkanQueueSetup {
	VQueueFamilyId family_id;
	int count;
};

struct VulkanDeviceSetup {
	vector<string> extensions;
	vector<VulkanQueueSetup> queues;
	Dynamic<VkPhysicalDeviceFeatures> features;
};

DEFINE_ENUM(CommandPoolFlag, transient, reset_command, protected_);
using CommandPoolFlags = EnumFlags<CommandPoolFlag>;

class VulkanCommandPool : public VulkanObjectBase<VulkanCommandPool> {
  public:
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

DEFINE_ENUM(VMemoryFlag, device_local, host_visible, host_coherent, host_cached, lazily_allocated,
			protected_);
using VMemoryFlags = EnumFlags<VMemoryFlag>;

class VulkanDeviceMemory : public VulkanObjectBase<VulkanDeviceMemory> {
  public:
  private:
	friend class VulkanDevice;
	VulkanDeviceMemory(VkDeviceMemory, VObjectId, u64 size, VMemoryFlags);
	~VulkanDeviceMemory();

	u64 m_size;
	VMemoryFlags m_flags;
};

class VulkanDevice {
  public:
	Ex<PVSemaphore> createSemaphore(bool is_signaled = false);
	Ex<PVFence> createFence(bool is_signaled = false);
	Ex<PVShaderModule> createShaderModule(CSpan<char> bytecode);
	Ex<PVCommandPool> createCommandPool(VQueueFamilyId, CommandPoolFlags);
	Ex<PVDeviceMemory> allocDeviceMemory(u64 size, u32 memory_type_bits, VMemoryFlags);

	VDeviceRef ref() const { return VDeviceRef(m_id); }
	VDeviceId id() const { return m_id; }
	VPhysicalDeviceId physId() const { return m_phys_id; }
	VkDevice handle() const { return m_handle; }
	VkPhysicalDevice physHandle() const { return m_phys_handle; }

	CSpan<Pair<VkQueue, VQueueFamilyId>> queues() const;

	template <class THandle, class... Args>
	VPtr<THandle> createObject(THandle handle, Args &&...args) {
		using Object = typename VulkanHandleInfo<THandle>::Type;
		auto [ptr, object_id] = allocObject<Object>();
		auto *object = new(ptr) Object(handle, object_id, std::forward<Args>(args)...);
		return {object};
	}

	static constexpr int max_defer_frames = 3;
	using ReleaseFunc = void (*)(void *, VkDevice);
	void deferredRelease(void *ptr, ReleaseFunc, int num_frames = max_defer_frames);
	void nextReleasePhase();

  private:
	template <class TObject> Pair<void *, VObjectId> allocObject();
	template <class TObject> void destroyObject(VulkanObjectBase<TObject> *);

  private:
	VulkanDevice(VDeviceId, VPhysicalDeviceId, VInstanceRef);
	~VulkanDevice();

	VulkanDevice(const VulkanDevice &) = delete;
	void operator=(const VulkanDevice &) = delete;

	Ex<void> initialize(const VulkanDeviceSetup &);

	friend class VulkanInstance;
	friend class VulkanStorage;
	template <class T> friend class VulkanObjectBase;

	struct Impl;
	Dynamic<Impl> m_impl;

	VkDevice m_handle = nullptr;
	VkPhysicalDevice m_phys_handle = nullptr;
	VInstanceRef m_instance_ref;
	VPhysicalDeviceId m_phys_id;
	VDeviceId m_id;
};
}