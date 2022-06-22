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
			g_storage.counters[m_id.objectId()]++;
	}
	VLightPtr(VLightPtr &&rhs) : m_id(rhs.m_id) { rhs.id = {}; }
	~VLightPtr() { g_manager.release(m_id); }

	void operator=(const VLightPtr &rhs) {
		g_storage.assignRef(m_id, rhs.m_id);
		m_id = rhs.m_id;
	}

	void operator=(VLightPtr &&rhs) {
		if(&rhs != this) {
			swap(m_id, rhs.m_id);
			rhs.reset();
		}
	}

	void swap(VLightPtr &rhs) { fwk::swap(m_id, rhs.m_id); }

	bool valid() const { return m_id.valid(); }
	explicit operator bool() const { return m_id.valid(); }

	T operator*() const { return PASSERT(valid()), g_manager.handles[m_id.objectId()]; }

	operator VulkanObjectId() const { return m_id; }
	VDeviceId deviceId() const { return m_id.deviceId(); }
	int objectId() const { return m_id.objectId(); }
	int refCount() const { return PASSERT(valid()), g_manager.counters[m_id.objectId()]; }

	void reset() {
		if(m_bits != 0) {
			g_manager.release(*this);
			m_bits = 0;
		}
	}

	bool operator==(const VLightPtr &rhs) const { return m_bits == rhs.m_bits; }
	bool operator<(const VLightPtr &rhs) const { return m_bits < rhs.m_bits; }

  private:
	static constexpr VulkanObjectManager &g_manager = VulkanInstance::g_obj_managers[int(type_id)];
	friend T;

	VLightPtr(VulkanObjectId id, NoRefIncreaseTag) : m_id(id) {}

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
			g_storage.counters[m_id.objectId()]++;
	}
	VWrapPtr(VWrapPtr &&rhs) : m_id(rhs.m_id) { rhs.id = {}; }
	~VWrapPtr() { g_manager.release(m_id); }

	void operator=(const VWrapPtr &rhs) {
		g_storage.assignRef(m_id, rhs.m_id);
		m_id = rhs.m_id;
	}

	void operator=(VWrapPtr &&rhs) {
		if(&rhs != this) {
			swap(m_id, rhs.m_id);
			rhs.reset();
		}
	}

	void swap(VWrapPtr &rhs) { fwk::swap(m_id, rhs.m_id); }

	/*
	T &operator*() const {
		PASSERT(valid());
		return g_storage.objects[id()];
	}
	T *operator->() const {
		PASSERT(valid());
		return &g_storage.objects[id()];
	}*/

	bool valid() const { return m_id.valid(); }
	explicit operator bool() const { return m_id.valid(); }

	operator VulkanObjectId() const { return m_id; }
	VDeviceId deviceId() const { return m_id.deviceId(); }
	int objectId() const { return m_id.objectId(); }
	T handle() const { return PASSERT(valid()), g_manager.handles[m_id.objectId()]; }
	int refCount() const { return PASSERT(valid()), g_manager.counters[m_id.objectId()]; }

	void reset() {
		if(m_bits != 0) {
			g_manager.release(*this);
			m_bits = 0;
		}
	}

	/*template <class... Args> void emplace(Args &&...args) {
		*this = T::make(std::forward<Args>(args)...);
	}

	template <class... Args,
			  class Ret = decltype(DECLVAL(T &)(std::forward<Args>(DECLVAL(Args &&))...))>
	Ret operator()(Args &&...args) const {
		PASSERT(valid());
		return g_storage.objects[id()](std::forward<Args>(args)...);
	}

	template <class Arg, class Ret = decltype(DECLVAL(T &)[std::forward<Arg>(DECLVAL(Arg &&))])>
	Ret operator[](Arg &&arg) const {
		PASSERT(valid());
		return g_storage.objects[id()][std::forward<Arg>(arg)];
	}*/

	bool operator==(const VWrapPtr &rhs) const { return m_bits == rhs.m_bits; }
	bool operator<(const VWrapPtr &rhs) const { return m_bits < rhs.m_bits; }

  private:
	static constexpr VulkanObjectManager &g_manager = VulkanInstance::g_obj_managers[int(type_id)];
	friend T;

	VWrapPtr(VulkanObjectId id, NoRefIncreaseTag) : m_id(id) {}

	VulkanObjectId m_id;
};
}
