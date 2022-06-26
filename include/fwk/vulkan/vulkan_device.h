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

	CSpan<Pair<VkQueue, VQueueFamilyId>> queues() const { return m_queues; }

	template <class THandle, class... Args>
	VPtr<THandle> createObject(THandle handle, Args &&...args) {
		using Object = typename VulkanTypeInfo<THandle>::Wrapper;
		auto [ptr, object_id] = allocObject<THandle>();
		auto *object = new(ptr) Object(handle, object_id, std::forward<Args>(args)...);
		return {object};
	}

	static constexpr int max_defer_frames = 3;
	using ReleaseFunc = void (*)(void *, VkDevice);
	void deferredRelease(void *ptr, ReleaseFunc, int num_frames = max_defer_frames);
	void nextReleasePhase();

  private:
	template <class THandle> Pair<void *, VObjectId> allocObject() {
		auto type_id = VulkanTypeInfo<THandle>::type_id;
		using Object = typename VulkanTypeInfo<THandle>::Wrapper;
		using Slab = VulkanObjectSlab<Object>;

		auto &slabs = m_slabs[type_id];
		auto &slab_list = m_fillable_slabs[type_id];
		if(slab_list.empty()) {
			auto *new_slab = new Slab;
			u32 slab_id = u32(slabs.size());

			// Object index 0 is reserved as invalid
			if(slab_id == 0)
				new_slab->usage_bits |= 1u;

			new_slab->slab_id = slab_id;
			slabs.emplace_back(new_slab);
			slab_list.emplace_back(slab_id);
		}

		int slab_idx = slab_list.back();
		auto *slab = reinterpret_cast<Slab *>(slabs[slab_idx]);
		int local_idx = countTrailingZeros(~slab->usage_bits);
		slab->usage_bits |= (1u << local_idx);
		int object_id = local_idx + slab_idx * Slab::size;
		return {&slab->objects[local_idx], VObjectId(m_id, object_id)};
	}

	template <class TObject> void destroyObject(VulkanObjectBase<TObject> *ptr) {
		using Handle = typename VulkanObjectBase<TObject>::Handle;
		auto type_id = VulkanTypeInfo<Handle>::type_id;
		auto object_idx = ptr->objectId().objectIdx();
		int local_idx = object_idx & ((1 << vulkan_slab_size_log2) - 1);
		auto *slab_data = m_slabs[type_id][object_idx >> vulkan_slab_size_log2];
		auto *slab = reinterpret_cast<VulkanObjectSlab<TObject> *>(slab_data);

		reinterpret_cast<TObject *>(ptr)->~TObject();

		// TODO: move usage bits out of slabs data storage
		if(slab->usage_bits == ~0u)
			m_fillable_slabs[type_id].emplace_back(slab->slab_id);
		slab->usage_bits &= ~(1u << local_idx);
	}

  private:
	VulkanDevice(VDeviceId, VPhysicalDeviceId, VInstanceRef);
	~VulkanDevice();

	VulkanDevice(const VulkanDevice &) = delete;
	void operator=(const VulkanDevice &) = delete;

	Ex<void> initialize(const VulkanDeviceSetup &);

	friend class VulkanInstance;
	friend class VulkanStorage;
	template <class T> friend class VulkanObjectBase;

	// TODO: trim empty unused slabs from time to time
	// we can't move them around, but we can free the slab and
	// leave nullptr in slab array

	EnumMap<VTypeId, vector<void *>> m_slabs;
	EnumMap<VTypeId, vector<u32>> m_fillable_slabs;
	vector<Pair<void *, ReleaseFunc>> m_to_release[max_defer_frames + 1];

	// TODO: signify which queue is for what?
	vector<Pair<VkQueue, VQueueFamilyId>> m_queues;

	VkDevice m_handle = nullptr;
	VkPhysicalDevice m_phys_handle = nullptr;

	VPhysicalDeviceId m_phys_id;
	VDeviceId m_id;
	VInstanceRef m_instance_ref;
};

template <class T> void VulkanObjectBase<T>::decRefCount() {
	PASSERT(m_ref_count > 0);
	m_ref_count--;
	if(m_ref_count == 0) {
		auto &device = reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[deviceId()]);
		device.destroyObject(this);
	}
}

template <class T> void VulkanObjectBase<T>::deferredHandleRelease(ReleaseFunc release_func) {
	auto &device = reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[deviceId()]);
	device.deferredRelease(m_handle, release_func);
}
}