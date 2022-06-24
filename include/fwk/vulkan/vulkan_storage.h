// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_map.h"
#include "fwk/pod_vector.h"
#include "fwk/vulkan_base.h"

namespace fwk {

// -----------------------------------------------------------------------------------------------
// ----------  Reference, pointer & ID classes  --------------------------------------------------

class VObjectId {
  public:
	VObjectId() : m_bits(0) {}
	VObjectId(VDeviceId device_id, int object_id)
		: m_bits(u32(object_id) | (u32(device_id) << 28)) {
		PASSERT(object_id >= 0 && object_id <= 0xfffffff);
		PASSERT(uint(device_id) < 16);
	}
	explicit VObjectId(u32 bits) : m_bits(bits) {}

	bool valid() const { return m_bits != 0; }
	explicit operator bool() const { return m_bits != 0; }

	int objectId() const { return int(m_bits & 0xfffffff); }
	VDeviceId deviceId() const { return VDeviceId(m_bits >> 28); }

	bool operator==(const VObjectId &rhs) const { return m_bits == rhs.m_bits; }
	bool operator<(const VObjectId &rhs) const { return m_bits < rhs.m_bits; }

  private:
	u32 m_bits;
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
	using Wrapper = typename VulkanTypeInfo<T>::Wrapper;
	static constexpr VTypeId type_id = VulkanTypeInfo<T>::type_id;

	VPtr() = default;
	VPtr(const VPtr &);
	VPtr(VPtr &&);
	~VPtr();

	void operator=(const VPtr &rhs);
	void operator=(VPtr &&rhs);

	void swap(VPtr &);

	template <class U = T, EnableIf<vk_type_wrapped<U>>...> Wrapper &operator*() const;
	template <class U = T, EnableIf<vk_type_wrapped<U>>...> Wrapper *operator->() const;

	bool valid() const { return m_id.valid(); }
	explicit operator bool() const { return m_id.valid(); }

	operator VObjectId() const { return m_id; }
	operator T() const { return handle(); }
	VDeviceId deviceId() const { return m_id.deviceId(); }
	int objectId() const { return m_id.objectId(); }

	T handle() const;
	void reset();

	bool operator==(const VPtr &) const;
	bool operator<(const VPtr &) const;

  private:
	friend VulkanStorage;

	VObjectId m_id;
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

	static constexpr int device_size = 40, device_alignment = 8;
	static constexpr int instance_size = 32, instance_alignment = 8;
	using DeviceStorage = std::aligned_storage_t<device_size, device_alignment>;
	using InstanceStorage = std::aligned_storage_t<instance_size, instance_alignment>;

	template <class THandle, class... Args>
	VPtr<THandle> allocObject(VDeviceRef, THandle, Args &&...);
	template <class THandle, class TWrapper> TWrapper &accessWrapper(VObjectId);

  private:
	friend class VInstanceRef;
	friend class VDeviceRef;
	friend class VWindowRef;

	friend class VulkanInstance;
	friend class VulkanDevice;
	friend class VulkanWindow;
	template <class> friend class VPtr;

	friend class VulkanImage;

	Ex<VInstanceRef> allocInstance();
	Ex<VDeviceRef> allocDevice(VInstanceRef, VPhysicalDeviceId);
	Ex<VWindowRef> allocWindow(VInstanceRef);

	void incInstanceRef();
	void decInstanceRef();
	void incRef(VDeviceId);
	void decRef(VDeviceId);
	void incRef(VWindowId);
	void decRef(VWindowId);

	template <class THandle> void incRef(VObjectId);
	template <class THandle> void decRef(VObjectId);

	// This is useful for externally created objects; Only wrapper object will be destroyed;
	// This function can only be called from Wrapper's destructor
	template <class THandle, class TWrapper> void disableHandleDestruction(const TWrapper *);

	void nextReleasePhase(VDeviceId, VkDevice);

	using ObjectDestructor = void (*)(void *, VkDevice);

	struct ObjectStorage {
		VObjectId addHandle(VDeviceId, void *);
		template <class THandle> VObjectId addObject(VDeviceRef, THandle);
		template <class TWrapper> void resizeObjectData(int new_capacity);
		void nextReleasePhase(VTypeId, VDeviceId, VkDevice);

		vector<u32> counters;
		vector<void *> handles;
		PodVector<u8> wrapper_data;
		Array<u32, num_release_phases> to_be_released_lists[max_devices] = {};
		ObjectDestructor destructor = nullptr;
		u32 free_list = 0;
	};

	DeviceStorage devices[max_devices];
	InstanceStorage instance;
	EnumMap<VTypeId, ObjectStorage> objects;
	vector<Pair<VulkanWindow *, int>> windows;

	int device_ref_counts[max_devices] = {};
	int instance_ref_count = 0;
};

extern VulkanStorage g_vk_storage;

// -------------------------------------------------------------------------------------------
// ----------  IMPLEMENTATION  ---------------------------------------------------------------

template <class THandle> inline void VulkanStorage::incRef(VObjectId id) {
	if(id) {
		auto type_id = VulkanTypeInfo<THandle>::type_id;
		objects[type_id].counters[id.objectId()]++;
	}
}

template <class THandle, class... Args>
VPtr<THandle> VulkanStorage::allocObject(VDeviceRef device, THandle handle, Args &&...args) {
	using Wrapper = typename VulkanTypeInfo<THandle>::Wrapper;
	auto type_id = VulkanTypeInfo<THandle>::type_id;
	auto id = objects[type_id].addObject(device, handle);
	if constexpr(!is_same<Wrapper, None>)
		new(&accessWrapper<THandle, Wrapper>(id)) Wrapper(std::forward<Args>(args)...);
	VPtr<THandle> out;
	out.m_id = id;
	return out;
}

template <class THandle, class TWrapper>
inline TWrapper &VulkanStorage::accessWrapper(VObjectId id) {
	auto type_id = VulkanTypeInfo<THandle>::type_id;
	auto *bytes = objects[type_id].wrapper_data.data();
	return reinterpret_cast<TWrapper *>(bytes)[id.objectId()];
}

FWK_ALWAYS_INLINE VulkanDevice &VDeviceRef::operator*() const {
	return reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[m_id]);
}
FWK_ALWAYS_INLINE VulkanDevice *VDeviceRef::operator->() const {
	return reinterpret_cast<VulkanDevice *>(&g_vk_storage.devices[m_id]);
}

template <class T> inline VPtr<T>::VPtr(const VPtr &rhs) : m_id(rhs.m_id) {
	g_vk_storage.incRef<T>(m_id);
}
template <class T> inline VPtr<T>::VPtr(VPtr &&rhs) : m_id(rhs.m_id) { rhs.m_id = {}; }
template <class T> VPtr<T>::~VPtr() { g_vk_storage.decRef<T>(m_id); }

template <class T> void VPtr<T>::operator=(const VPtr &rhs) {
	if(m_id != rhs.m_id) {
		g_vk_storage.decRef<T>(m_id);
		g_vk_storage.incRef(rhs.m_id);
		m_id = rhs.m_id;
	}
}

template <class T> void VPtr<T>::operator=(VPtr &&rhs) {
	if(&rhs != this) {
		fwk::swap(m_id, rhs.m_id);
		rhs.reset();
	}
}

template <class T> inline void VPtr<T>::swap(VPtr &rhs) { fwk::swap(m_id, rhs.m_id); }

template <class T>
template <class U, EnableIf<vk_type_wrapped<U>>...>
FWK_ALWAYS_INLINE auto VPtr<T>::operator*() const -> Wrapper & {
	PASSERT(valid());
	return g_vk_storage.accessWrapper<T, Wrapper>(m_id);
}

template <class T>
template <class U, EnableIf<vk_type_wrapped<U>>...>
FWK_ALWAYS_INLINE auto VPtr<T>::operator->() const -> Wrapper * {
	PASSERT(valid());
	return &g_vk_storage.accessWrapper<T, Wrapper>(m_id);
}

template <class T> FWK_ALWAYS_INLINE T VPtr<T>::handle() const {
	PASSERT(valid());
	return T(g_vk_storage.objects[type_id].handles[m_id.objectId()]);
}

template <class T> void VPtr<T>::reset() {
	if(m_id) {
		g_vk_storage.decRef<T>(m_id);
		m_id = {};
	}
}

template <class T> FWK_ALWAYS_INLINE bool VPtr<T>::operator==(const VPtr &rhs) const {
	return m_id.bits == rhs.m_id.bits;
}
template <class T> FWK_ALWAYS_INLINE bool VPtr<T>::operator<(const VPtr &rhs) const {
	return m_id.bits < rhs.m_id.bits;
}

}