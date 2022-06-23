// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_storage.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_window.h"

#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_render_pass.h"

#include "vulkan/vulkan.h"

namespace fwk {

VulkanStorage g_vk_storage;

//UndefSize<VulkanInstance> test_size;
//UndefSize<VulkanDevice> test_size;
static_assert(sizeof(VulkanInstance) == VulkanStorage::instance_size);
static_assert(alignof(VulkanInstance) == VulkanStorage::instance_alignment);
static_assert(sizeof(VulkanDevice) == VulkanStorage::device_size);
static_assert(alignof(VulkanDevice) == VulkanStorage::device_alignment);

VulkanStorage::VulkanStorage() {
	ASSERT("VulkanStorage is a singleton, don't create additional instances" &&
		   this == &g_vk_storage);
}

VulkanStorage::~VulkanStorage() {}

Ex<VulkanInstance *> VulkanStorage::allocInstance() {
	if(instance_ref_count > 0)
		return ERROR("VulkanInstance already created");
	return new(&instance) VulkanInstance;
}

Ex<VulkanDevice *> VulkanStorage::allocDevice(VInstanceRef instance_ref,
											  VPhysicalDeviceId phys_id) {
	for(int i : intRange(max_devices))
		if(device_ref_counts[i] == 0)
			return new(&devices[i]) VulkanDevice(VDeviceId(i), phys_id, instance_ref);
	return ERROR("Too many VulkanDevices created (max: %)", max_devices);
}

Ex<VWindowRef> VulkanStorage::allocWindow(VInstanceRef instance_ref) {
	int window_id = -1;
	for(int i : intRange(windows))
		if(windows[i].first == nullptr)
			window_id = i;
	if(window_id == -1) {
		if(windows.size() >= max_windows)
			return ERROR("Too many VulkanWindows created (max: %)", max_windows);
		window_id = windows.size();
		windows.emplace_back(nullptr, 0);
	}

	VWindowId id(window_id);
	auto *window = new VulkanWindow(id, instance_ref);
	windows[window_id].first = window;
	return VWindowRef(id, window);
}

void VulkanStorage::incInstanceRef() { instance_ref_count++; }
void VulkanStorage::decInstanceRef() {
	if(--instance_ref_count == 0)
		reinterpret_cast<VulkanInstance &>(instance).~VulkanInstance();
}

void VulkanStorage::incRef(VDeviceId id) { device_ref_counts[id]++; }
void VulkanStorage::decRef(VDeviceId id) {
	if(--device_ref_counts[id] == 0)
		reinterpret_cast<VulkanDevice &>(devices[id]).~VulkanDevice();
}

void VulkanStorage::incRef(VWindowId id) { windows[id].second++; }
void VulkanStorage::decRef(VWindowId id) {
	if(--windows[id].second == 0) {
		auto &ref = windows[id].first;
		delete ref;
		ref = nullptr;
	}
}

template <class T>
void resizeObjectData(CSpan<u32> ref_counts, PodVector<u8> &data, int new_capacity) {
	if(data.size() >= new_capacity)
		return;
	PodVector<u8> new_data(new_capacity * sizeof(T));
	// Moving objects to new memory range
	for(int i : intRange(ref_counts))
		if(ref_counts[i] != 0) {
			T &old_object = reinterpret_cast<T *>(data.data())[i];
			T &new_object = reinterpret_cast<T *>(new_data.data())[i];
			new(&new_object) T(move(old_object));
			old_object.~T();
		}
	data.swap(new_data);
}

void VulkanStorage::resizeObjectData(int new_capacity, VTypeId type_id) {
	int index = int(type_id);
	switch(type_id) {
#define CASE_WRAPPED_TYPE(Wrapper, VkType, type_id_)                                               \
	case VTypeId::type_id_:                                                                        \
		resizeObjectData<Wrapper>(obj_managers[index].counters, obj_data[index], new_capacity);    \
		break;
#include "fwk/vulkan/vulkan_types.h"

	default:
		break;
	}
}

VInstanceRef::VInstanceRef() { g_vk_storage.incInstanceRef(); }
VInstanceRef::VInstanceRef(const VInstanceRef &) { g_vk_storage.incInstanceRef(); }
VInstanceRef::~VInstanceRef() { g_vk_storage.decInstanceRef(); }
void VInstanceRef::operator=(const VInstanceRef &) {}

VulkanInstance &VInstanceRef::operator*() const {
	return reinterpret_cast<VulkanInstance &>(g_vk_storage.instance);
}
VulkanInstance *VInstanceRef::operator->() const {
	return reinterpret_cast<VulkanInstance *>(&g_vk_storage.instance);
}

VDeviceRef::VDeviceRef(VDeviceId id) : m_id(id) { g_vk_storage.incRef(id); }
VDeviceRef::VDeviceRef(const VDeviceRef &rhs) : m_id(rhs.m_id) { g_vk_storage.incRef(m_id); }
VDeviceRef::~VDeviceRef() { g_vk_storage.decRef(m_id); }
void VDeviceRef::operator=(const VDeviceRef &rhs) {
	if(m_id != rhs.m_id) {
		g_vk_storage.decRef(m_id);
		m_id = rhs.m_id;
		g_vk_storage.incRef(m_id);
	}
}

VWindowRef::VWindowRef(VWindowId id, VulkanWindow *ptr) : m_id(id), m_ptr(ptr) {
	g_vk_storage.incRef(m_id);
}
VWindowRef::VWindowRef(const VWindowRef &rhs) : m_id(rhs.m_id), m_ptr(rhs.m_ptr) {
	g_vk_storage.incRef(m_id);
}
VWindowRef::~VWindowRef() { g_vk_storage.decRef(m_id); }
void VWindowRef::operator=(const VWindowRef &rhs) {
	if(m_id != rhs.m_id) {
		g_vk_storage.decRef(m_id);
		m_id = rhs.m_id;
		m_ptr = rhs.m_ptr;
		g_vk_storage.incRef(m_id);
	}
}
}