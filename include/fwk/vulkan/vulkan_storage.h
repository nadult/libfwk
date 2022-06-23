// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vulkan/vulkan_object_manager.h"

namespace fwk {

struct VulkanStorage {
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
	static Ex<VulkanInstance *> allocInstance();
	static Ex<VulkanDevice *> allocDevice(VInstanceRef, VPhysicalDeviceId);
	static Ex<VWindowRef> allocWindow(VInstanceRef);

	static void incInstanceRef();
	static void decInstanceRef();

	static void incDeviceRef(VDeviceId);
	static void decDeviceRef(VDeviceId);

	static void incWindowRef(VWindowId);
	static void decWindowRef(VWindowId);

	// TODO: single static storage g_vulkan;
	// TODO: merge object manager here
	// TODO: move storages from VWrapPtr here
	static DeviceStorage g_devices[max_devices];
	static InstanceStorage g_instance;
	static VulkanObjectManager g_obj_managers[count<VTypeId>];
	static vector<Pair<VulkanWindow *, int>> g_windows;

	static int g_device_ref_counts[max_devices];
	static int g_instance_ref_count;
};

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
	return reinterpret_cast<VulkanDevice &>(VulkanStorage::g_devices[m_id]);
}
FWK_ALWAYS_INLINE VulkanDevice *VDeviceRef::operator->() const {
	return reinterpret_cast<VulkanDevice *>(&VulkanStorage::g_devices[m_id]);
}
}