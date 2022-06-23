// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_object_manager.h"

namespace fwk {

struct NoRefIncreaseTag {};

// TODO: description
template <class T> class VLightPtr {
  public:
	using Wrapper = typename VulkanTypeInfo<T>::Wrapper;
	static constexpr VTypeId type_id = VulkanTypeInfo<T>::type_id;

	VLightPtr() = default;
	VLightPtr(const VLightPtr &rhs) : m_id(rhs.m_id) {
		if(m_id)
			g_manager.counters[m_id.objectId()]++;
	}
	VLightPtr(VLightPtr &&rhs) : m_id(rhs.m_id) { rhs.m_id = {}; }
	~VLightPtr() { g_manager.release(m_id); }

	void operator=(const VLightPtr &rhs) {
		g_manager.assignRef(m_id, rhs.m_id);
		m_id = rhs.m_id;
	}

	void operator=(VLightPtr &&rhs) {
		if(&rhs != this) {
			::swap(m_id, rhs.m_id);
			rhs.reset();
		}
	}

	void swap(VLightPtr &rhs) { fwk::swap(m_id, rhs.m_id); }

	bool valid() const { return m_id.valid(); }
	explicit operator bool() const { return m_id.valid(); }

	T operator*() const { return PASSERT(valid()), T(g_manager.handles[m_id.objectId()]); }

	operator VulkanObjectId() const { return m_id; }
	VDeviceId deviceId() const { return m_id.deviceId(); }
	int objectId() const { return m_id.objectId(); }
	int refCount() const { return PASSERT(valid()), int(g_manager.counters[m_id.objectId()]); }

	template <class... Args> static VLightPtr make(VDeviceId dev_id, T handle) {
		VLightPtr out;
		out.m_id = g_manager.add(dev_id, handle);
		return out;
	}

	void reset() {
		if(m_id) {
			g_manager.release(m_id);
			m_id = {};
		}
	}

	bool operator==(const VLightPtr &rhs) const { return m_id.bits == rhs.m_id.bits; }
	bool operator<(const VLightPtr &rhs) const { return m_id.bits < rhs.m_id.bits; }

  private:
	static constexpr VulkanObjectManager &g_manager = VulkanInstance::g_obj_managers[int(type_id)];

	VulkanObjectId m_id;
};

// TODO: description
// Reference counted; object is destroyed when there is no VPtrs pointing to it
// Objects are stored in a vector (they may move when new object is created)
// All refs should be destroyed before GlDevice
template <class T> class VWrapPtr {
  public:
	using Wrapper = typename VulkanTypeInfo<T>::Wrapper;
	static constexpr VTypeId type_id = VulkanTypeInfo<T>::type_id;

	VWrapPtr() = default;
	VWrapPtr(const VWrapPtr &rhs) : m_id(rhs.m_id) {
		if(m_id)
			g_manager.counters[m_id.objectId()]++;
	}
	VWrapPtr(VWrapPtr &&rhs) : m_id(rhs.m_id) { rhs.m_id = {}; }
	~VWrapPtr() { release(); }

	void operator=(const VWrapPtr &rhs) {
		if(m_id != rhs.m_id) {
			release();
			g_manager.acquire(rhs.m_id);
			m_id = rhs.m_id;
		}
	}

	void operator=(VWrapPtr &&rhs) {
		if(&rhs != this) {
			swap(m_id, rhs.m_id);
			rhs.reset();
		}
	}

	void swap(VWrapPtr &rhs) { fwk::swap(m_id, rhs.m_id); }

	T &operator*() const { return PASSERT(valid()), g_objects[m_id.objectId()]; }
	T *operator->() const { return PASSERT(valid()), &g_objects[m_id.objectId()]; }

	bool valid() const { return m_id.valid(); }
	explicit operator bool() const { return m_id.valid(); }

	operator VulkanObjectId() const { return m_id; }
	VDeviceId deviceId() const { return m_id.deviceId(); }
	int objectId() const { return m_id.objectId(); }
	T handle() const { return PASSERT(valid()), T(g_manager.handles[m_id.objectId()]); }
	int refCount() const { return PASSERT(valid()), int(g_manager.counters[m_id.objectId()]); }

	void reset() {
		if(m_id) {
			release();
			m_id = 0;
		}
	}

	template <class... Args> static VWrapPtr make(VDeviceId dev_id, T handle, Args &&...args) {
		VWrapPtr out;
		out.m_id = g_manager.add(dev_id, handle);
		FATAL("first resize buffer as needed");
		new(&g_objects[out.m_id.objectId()]) Wrapper{std::forward<Args>(args)...};
		return out;
	}

	bool operator==(const VWrapPtr &rhs) const { return m_id.bits == rhs.m_id.bits; }
	bool operator<(const VWrapPtr &rhs) const { return m_id.bits < rhs.m_id.bits; }

  private:
	static constexpr VulkanObjectManager &g_manager = VulkanInstance::g_obj_managers[int(type_id)];
	static PodVector<Wrapper> g_objects;

	void release() {
		if(refCount() == 1)
			g_objects[m_id.objectId()].~Wrapper();
		g_manager.release(m_id);
	}

	VulkanObjectId m_id;
};
}
