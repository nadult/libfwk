// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vulkan_base.h"

namespace fwk {

// -----------------------------------------------------------------------------------------------
// ----------  Reference, pointer & ID classes  --------------------------------------------------

class VObjectId {
  public:
	VObjectId() : m_bits(0) {}
	VObjectId(VDeviceId device_id, int object_idx)
		: m_bits(u32(object_idx) | (u32(device_id) << 28)) {
		PASSERT(object_idx >= 0 && object_idx <= 0xfffffff);
		PASSERT(uint(device_id) < 16);
	}
	explicit VObjectId(u32 bits) : m_bits(bits) {}

	bool valid() const { return m_bits != 0; }
	explicit operator bool() const { return m_bits != 0; }

	int objectIdx() const { return int(m_bits & 0xfffffff); }
	VDeviceId deviceId() const { return VDeviceId(m_bits >> 28); }

	bool operator==(const VObjectId &rhs) const { return m_bits == rhs.m_bits; }
	bool operator<(const VObjectId &rhs) const { return m_bits < rhs.m_bits; }

  private:
	u32 m_bits;
};

// All vulkan objects are stored in slabs in VulkanDevice
template <class T> class VulkanObjectBase {
  public:
	using Handle = typename VulkanTypeToHandle<T>::Type;

	Handle handle() const { return m_handle; }
	VDeviceId deviceId() const { return m_object_id.deviceId(); }
	VObjectId objectId() const { return m_object_id; }

  protected:
	VulkanObjectBase(Handle handle, VObjectId object_id, uint initial_ref_count = 1)
		: m_handle(handle), m_object_id(object_id), m_ref_count(initial_ref_count) {
		PASSERT(handle);
	}
	// Vulkan objects are immovable
	VulkanObjectBase(const VulkanObjectBase &) = delete;
	void operator=(const VulkanObjectBase &) = delete;

	friend class VulkanDevice;
	template <class> friend class VPtr;

	VkDevice deviceHandle() const;
	using ReleaseFunc = void (*)(void *, VkDevice);
	void deferredHandleRelease(ReleaseFunc);

	void decRefCount() {
		PASSERT(m_ref_count > 0);
		m_ref_count--;
		if(m_ref_count == 0)
			destroyObject();
	}

	Handle m_handle;
	VObjectId m_object_id;
	u32 m_ref_count;

  private:
	void destroyObject();
};

class VInstanceRef {
  public:
	VInstanceRef(const VInstanceRef &);
	~VInstanceRef();
	void operator=(const VInstanceRef &);

	VulkanInstance &operator*() const;
	VulkanInstance *operator->() const;

  private:
	friend class VulkanStorage;
	friend class VulkanInstance;
	VInstanceRef();
};

// TODO: conversion to handle from VDeviceRef
// TODO: better name for VDeviceRef
class VDeviceRef {
  public:
	VDeviceRef(const VDeviceRef &);
	~VDeviceRef();
	void operator=(const VDeviceRef &);

	VulkanDevice &operator*() const;
	VulkanDevice *operator->() const;

	VDeviceId id() const { return m_id; }

  private:
	friend class VulkanStorage;
	friend class VulkanDevice;
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

// Reference counted pointer to object stored in VulkanStorage; When no VPtrs<>
// point to a given object, ref_counts drop to 0 and object is marked to be destroyed.
// Objects are stored in resize-able vector, so whenever some new object is created,
// old objects may be moved to a new location in memory. That's why it's better to always
// access vulkan objects with this VPtr<>.
// When VulkanDevice is destroyed there should be np VPtrs<> left to any object form this device.
template <class T> class VPtr {
  public:
	using Object = typename VulkanTypeInfo<T>::Wrapper;
	using BaseObject = VulkanObjectBase<Object>;
	static constexpr VTypeId type_id = VulkanTypeInfo<T>::type_id;

	VPtr() : m_ptr(nullptr) {}
	VPtr(const VPtr &);
	VPtr(VPtr &&);
	~VPtr();

	void operator=(const VPtr &rhs);
	void operator=(VPtr &&rhs);

	void swap(VPtr &);

	Object &operator*() const { return PASSERT(m_ptr), *m_ptr; }
	Object *operator->() const { return PASSERT(m_ptr), m_ptr; }

	bool valid() const { return m_ptr; }
	explicit operator bool() const { return m_ptr; }

	operator VObjectId() const { return m_id; }
	operator T() const { return PASSERT(m_ptr), handle(); }
	VDeviceId deviceId() const { return m_id.deviceId(); }
	int objectId() const { return m_id.objectId(); }

	T handle() const;
	void reset();

	bool operator==(const VPtr &) const;
	bool operator<(const VPtr &) const;

  private:
	VPtr(Object *ptr) : m_ptr(ptr) {}
	friend class VulkanDevice;

	Object *m_ptr;
};

// -------------------------------------------------------------------------------------------
// ----------  VulkanStorage class  ----------------------------------------------------------

// Manages lifetimes of Vulkan Instance, Devices, Windows and basic device-based objects.
// Objects are destroyed when ref-count drops to 0 and several (2-3 typically) release phases passed.
// Release phase changes when a frame has finished rendering.
class VulkanStorage {
  public:
	VulkanStorage();
	~VulkanStorage();

	VulkanStorage(const VulkanStorage &) = delete;
	void operator=(VulkanStorage &) = delete;

	static constexpr int max_devices = 4;
	static constexpr int max_windows = VWindowId::maxIndex() + 1;
	static constexpr int num_release_phases = 3;

	static constexpr int device_size = 616, device_alignment = 8;
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
	template <class> friend class VulkanObjectBase;

	Ex<VInstanceRef> allocInstance();
	Ex<VDeviceRef> allocDevice(VInstanceRef, VPhysicalDeviceId);
	Ex<VWindowRef> allocWindow(VInstanceRef);

	void incInstanceRef();
	void decInstanceRef();
	void incRef(VDeviceId);
	void decRef(VDeviceId);
	void incRef(VWindowId);
	void decRef(VWindowId);

	VkDevice device_handles[max_devices] = {};
	DeviceStorage devices[max_devices];
	InstanceStorage instance;
	vector<Pair<VulkanWindow *, int>> windows;

	int device_ref_counts[max_devices] = {};
	int instance_ref_count = 0;
};

extern VulkanStorage g_vk_storage;

// -------------------------------------------------------------------------------------------
// ----------  IMPLEMENTATION  ---------------------------------------------------------------

template <class T> FWK_ALWAYS_INLINE VkDevice VulkanObjectBase<T>::deviceHandle() const {
	return g_vk_storage.device_handles[deviceId()];
}

FWK_ALWAYS_INLINE VulkanDevice &VDeviceRef::operator*() const {
	return reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[m_id]);
}
FWK_ALWAYS_INLINE VulkanDevice *VDeviceRef::operator->() const {
	return reinterpret_cast<VulkanDevice *>(&g_vk_storage.devices[m_id]);
}

template <class T> inline VPtr<T>::VPtr(const VPtr &rhs) : m_ptr(rhs.m_ptr) {
	if(m_ptr)
		reinterpret_cast<BaseObject *>(m_ptr)->m_ref_count++;
}

template <class T> inline VPtr<T>::VPtr(VPtr &&rhs) : m_ptr(rhs.m_ptr) { rhs.m_ptr = nullptr; }
template <class T> VPtr<T>::~VPtr() {
	if(m_ptr)
		reinterpret_cast<BaseObject *>(m_ptr)->decRefCount();
}

template <class T> void VPtr<T>::operator=(const VPtr &rhs) {
	if(m_ptr == rhs.m_ptr)
		return;
	if(m_ptr)
		reinterpret_cast<BaseObject *>(m_ptr)->decRefCount();
	m_ptr = rhs.m_ptr;
	if(m_ptr)
		reinterpret_cast<BaseObject *>(m_ptr)->m_ref_count++;
}

template <class T> void VPtr<T>::operator=(VPtr &&rhs) {
	if(&rhs != this) {
		fwk::swap(m_ptr, rhs.m_ptr);
		rhs.reset();
	}
}

template <class T> inline void VPtr<T>::swap(VPtr &rhs) { fwk::swap(m_id, rhs.m_id); }

template <class T> void VPtr<T>::reset() {
	if(m_ptr) {
		reinterpret_cast<BaseObject *>(m_ptr)->decRefCount();
		m_ptr = nullptr;
	}
}

template <class T> FWK_ALWAYS_INLINE T VPtr<T>::handle() const {
	return m_ptr ? reinterpret_cast<BaseObject *>(m_ptr)->m_handle : nullptr;
}

template <class T> FWK_ALWAYS_INLINE bool VPtr<T>::operator==(const VPtr &rhs) const {
	return m_ptr == rhs.m_ptr;
}

template <class T> FWK_ALWAYS_INLINE bool VPtr<T>::operator<(const VPtr &rhs) const {
	VObjectId lhs_id, rhs_id;
	if(m_ptr)
		lsh_id = m_ptr->objectId();
	if(rhs.m_ptr)
		rhs_id = rhs.m_ptr->objectId();
	return lhs_id < rhs_id;
}
}