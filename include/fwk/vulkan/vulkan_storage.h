// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vulkan/vulkan_object_manager.h"

#include "fwk/pod_vector.h"

namespace fwk {

class VulkanStorage {
  public:
	VulkanStorage();
	~VulkanStorage();

	VulkanStorage(const VulkanStorage &) = delete;
	void operator=(VulkanStorage &) = delete;

	static constexpr int max_devices = 4;
	static constexpr int max_windows = VWindowId::maxIndex() + 1;

	static constexpr int device_size = 40, device_alignment = 8;
	static constexpr int instance_size = 32, instance_alignment = 8;
	using DeviceStorage = std::aligned_storage_t<device_size, device_alignment>;
	using InstanceStorage = std::aligned_storage_t<instance_size, instance_alignment>;

  private:
	friend class VInstanceRef;
	friend class VDeviceRef;
	friend class VWindowRef;

	friend class VulkanInstance;
	friend class VulkanDevice;
	friend class VulkanWindow;

	template <class> friend class VLightPtr;
	template <class> friend class VWrapPtr;

	// TODO: return refs
	Ex<VulkanInstance *> allocInstance();
	Ex<VulkanDevice *> allocDevice(VInstanceRef, VPhysicalDeviceId);
	Ex<VWindowRef> allocWindow(VInstanceRef);

	VulkanObjectId addObject(VDeviceId device_id, VTypeId type_id, void *handle);

	void incInstanceRef();
	void decInstanceRef();
	void incRef(VDeviceId);
	void decRef(VDeviceId);
	void incRef(VWindowId);
	void decRef(VWindowId);

	void resizeObjectData(int new_capacity, VTypeId);

	template <class T> T &accessObject(int index, VTypeId type_id) {
		auto *bytes = obj_data[int(type_id)].data();
		return reinterpret_cast<T *>(bytes)[index];
	}

	// TODO: merge object manager here
	// TODO: move storages from VWrapPtr here
	DeviceStorage devices[max_devices];
	InstanceStorage instance;
	PodVector<u8> obj_data[count<VTypeId>];
	VulkanObjectManager obj_managers[count<VTypeId>];
	int wrapped_type_sizes[count<VTypeId>];
	vector<Pair<VulkanWindow *, int>> windows;
	int device_ref_counts[max_devices] = {};
	int instance_ref_count = 0;
};

extern VulkanStorage g_vk_storage;

class VInstanceRef {
  public:
	VInstanceRef(const VInstanceRef &);
	~VInstanceRef();
	void operator=(const VInstanceRef &);

	VulkanInstance &operator*() const;
	VulkanInstance *operator->() const;

  private:
	friend class VulkanInstance;
	VInstanceRef();
};

class VDeviceRef {
  public:
	VDeviceRef(const VDeviceRef &);
	~VDeviceRef();
	void operator=(const VDeviceRef &);

	VulkanDevice &operator*() const;
	VulkanDevice *operator->() const;

	VDeviceId id() const { return m_id; }

  private:
	friend class VulkanInstance;
	VDeviceRef(VDeviceId);

	VDeviceId m_id;
};

class VWindowRef {
  public:
	VWindowRef(const VWindowRef &);
	~VWindowRef();
	void operator=(const VWindowRef &);

	VulkanWindow &operator*() const { return *m_ptr; }
	VulkanWindow *operator->() const { return m_ptr; }
	VWindowId id() const { return m_id; }

  private:
	friend class VulkanStorage;
	VWindowRef(VWindowId, VulkanWindow *);

	VWindowId m_id;
	VulkanWindow *m_ptr;
};

FWK_ALWAYS_INLINE VulkanDevice &VDeviceRef::operator*() const {
	return reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[m_id]);
}
FWK_ALWAYS_INLINE VulkanDevice *VDeviceRef::operator->() const {
	return reinterpret_cast<VulkanDevice *>(&g_vk_storage.devices[m_id]);
}
}