// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_object_manager.h"

namespace fwk {

struct NoRefIncreaseTag {};

// Identifies object stored in VulkanStorage
// Reference counted; object is destroyed when there is no VPtrs pointing to it
// Objects are stored in a vector (they may move when new object is created)
// All refs should be destroyed before GlDevice
template <class T> class VPtr {
  public:
	using Wrapper = typename VulkanTypeInfo<T>::Wrapper;
	static constexpr VTypeId type_id = VulkanTypeInfo<T>::type_id;

	VPtr() : m_id(0) {}
	VPtr(const VPtr &rhs) : m_bits(rhs.m_bits) {
		if(m_bits != 0)
			g_storage.counters[objectId()]++;
	}
	VPtr(VPtr &&rhs) : m_bits(rhs.m_bits) { rhs.m_bits = 0; }
	~VPtr() {
		if(m_bits != 0)
			g_manager.release(VulkanObjectId(m_bits));
	}

	void operator=(const VPtr &rhs) {
		if(&rhs == this)
			return;
		reset();
		m_bits = rhs.m_bits;
		if(m_bits != 0)
			g_storage.counters[objectId()]++;
	}

	void operator=(VPtr &&rhs) {
		if(&rhs != this) {
			swap(m_bits, rhs.m_bits);
			rhs.reset();
		}
	}

	void swap(VPtr &rhs) { fwk::swap(m_bits, rhs.m_bits); }

	/*
	T &operator*() const {
		PASSERT(valid());
		return g_storage.objects[id()];
	}
	T *operator->() const {
		PASSERT(valid());
		return &g_storage.objects[id()];
	}*/

	bool valid() const { return m_bits != 0; }
	explicit operator bool() const { return m_id != 0; }

	operator VulkanObjectId() const { return VulkanObjectId(m_bits); }
	VDeviceId deviceId() const { return m_bits >> device_id_bit_shift; }
	int objectId() const { return int(m_bits & id_bit_mask); }
	T handle() const { return PASSERT(valid()), g_manager.handles[objectId()]; }
	int refCount() const { return PASSERT(valid()), g_manager.counters[objectId()]; }

	void reset() {
		if(m_bits != 0) {
			g_manager.release(VulkanObjectId(m_bits));
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

	bool operator==(const VPtr &rhs) const { return m_bits == rhs.m_bits; }
	bool operator<(const VPtr &rhs) const { return m_bits < rhs.m_bits; }

  private:
	static constexpr int id_bit_mask = 0xfffffff;
	static constexpr int device_id_bit_shift = 4;

	static constexpr int max_ids = id_bit_mask + 1;
	static constexpr int max_device_ids = (1 << device_id_bit_shift);
	static constexpr VulkanObjectManager &g_manager = VulkanInstance::g_obj_managers[int(type_id)];

	friend T;
	VPtr(VulkanObjectId id, NoRefIncreaseTag) : m_bits(id.bits) {}

	void decRefs() {}

	uint m_bits;
};
}
