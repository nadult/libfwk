// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_map.h"
#include "fwk/pod_vector.h"
#include "fwk/vulkan_base.h"

namespace fwk {

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
	VPtr<THandle> allocObject(VDeviceRef device, THandle handle, Args &&...);
	template <class THandle, class TWrapper> TWrapper &accessWrapper(VObjectId id) {
		auto type_id = VulkanTypeInfo<THandle>::type_id;
		auto *bytes = objects[type_id].wrapper_data.data();
		return reinterpret_cast<TWrapper *>(bytes)[id.objectId()];
	}

  private:
	friend class VInstanceRef;
	friend class VDeviceRef;
	friend class VWindowRef;

	friend class VulkanInstance;
	friend class VulkanDevice;
	friend class VulkanWindow;
	template <class> friend class VPtr;

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
	void nextReleasePhase(VDeviceId, VkDevice);

	struct ObjectStorage {
		VObjectId addHandle(VDeviceId, void *);
		template <class THandle> VObjectId addObject(VDeviceRef, THandle);
		template <class TWrapper> void resizeObjectData(int new_capacity);
		void nextReleasePhase(VTypeId, VDeviceId, VkDevice);

		vector<u32> counters;
		vector<void *> handles;
		PodVector<u8> wrapper_data;
		Array<u32, num_release_phases> to_be_released_lists[max_devices] = {};
		u32 free_list = 0;
		uint wrapped_type_size = 0;
	};

	DeviceStorage devices[max_devices];
	InstanceStorage instance;
	EnumMap<VTypeId, ObjectStorage> objects;
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

// TODO: description
// Reference counted; object is destroyed when there is no VPtrs pointing to it
// Objects are stored in a vector (they may move when new object is created)
// All refs should be destroyed before GlDevice
template <class T> class VPtr {
  public:
	using Wrapper = typename VulkanTypeInfo<T>::Wrapper;
	template <class U>
	using EnableIfWrapped = EnableIf<!is_same<typename VulkanTypeInfo<U>::Wrapper, None>>;
	static constexpr VTypeId type_id = VulkanTypeInfo<T>::type_id;

	VPtr() = default;
	VPtr(const VPtr &rhs) : m_id(rhs.m_id) { g_vk_storage.incRef<T>(m_id); }
	VPtr(VPtr &&rhs) : m_id(rhs.m_id) { rhs.m_id = {}; }
	~VPtr() { g_vk_storage.decRef<T>(m_id); }

	void operator=(const VPtr &rhs) {
		if(m_id != rhs.m_id) {
			g_vk_storage.decRef<T>(m_id);
			g_vk_storage.incRef(rhs.m_id);
			m_id = rhs.m_id;
		}
	}

	void operator=(VPtr &&rhs) {
		if(&rhs != this) {
			fwk::swap(m_id, rhs.m_id);
			rhs.reset();
		}
	}

	void swap(VPtr &rhs) { fwk::swap(m_id, rhs.m_id); }

	template <class U = T, EnableIfWrapped<U>...> Wrapper &operator*() const {
		PASSERT(valid());
		return g_vk_storage.accessWrapper<T, Wrapper>(m_id);
	}
	template <class U = T, EnableIfWrapped<U>...> Wrapper *operator->() const {
		PASSERT(valid());
		return &g_vk_storage.accessWrapper<T, Wrapper>(m_id);
	}

	bool valid() const { return m_id.valid(); }
	explicit operator bool() const { return m_id.valid(); }

	operator VObjectId() const { return m_id; }
	operator T() const { return handle(); }
	VDeviceId deviceId() const { return m_id.deviceId(); }
	int objectId() const { return m_id.objectId(); }

	T handle() const {
		PASSERT(valid());
		return T(g_vk_storage.objects[type_id].handles[m_id.objectId()]);
	}

	void reset() {
		if(m_id) {
			g_vk_storage.decRef<T>(m_id);
			m_id = {};
		}
	}

	bool operator==(const VPtr &rhs) const { return m_id.bits == rhs.m_id.bits; }
	bool operator<(const VPtr &rhs) const { return m_id.bits < rhs.m_id.bits; }

  private:
	friend VulkanStorage;

	VObjectId m_id;
};

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

FWK_ALWAYS_INLINE VulkanDevice &VDeviceRef::operator*() const {
	return reinterpret_cast<VulkanDevice &>(g_vk_storage.devices[m_id]);
}
FWK_ALWAYS_INLINE VulkanDevice *VDeviceRef::operator->() const {
	return reinterpret_cast<VulkanDevice *>(&g_vk_storage.devices[m_id]);
}
}