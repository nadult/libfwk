// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_storage.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_window.h"

#include "vulkan/vulkan.h"

namespace fwk {

//UndefSize<VulkanInstance> test_size;
//UndefSize<VulkanDevice> test_size;
static_assert(sizeof(VulkanInstance) == VulkanStorage::instance_size);
static_assert(alignof(VulkanInstance) == VulkanStorage::instance_alignment);
static_assert(sizeof(VulkanDevice) == VulkanStorage::device_size);
static_assert(alignof(VulkanDevice) == VulkanStorage::device_alignment);

VulkanObjectManager VulkanStorage::g_obj_managers[count<VTypeId>];
VulkanStorage::DeviceStorage VulkanStorage::g_devices[max_devices];
VulkanStorage::InstanceStorage VulkanStorage::g_instance;
vector<Pair<VulkanWindow *, int>> VulkanStorage::g_windows;
#define CASE_WRAP_OBJECT(Wrapper, VkType, type_id)                                                 \
	PodVector<Wrapper> VulkanStorage::g_##type_id##_objects;
#include "fwk/vulkan/vulkan_types.h"

int VulkanStorage::g_device_ref_counts[max_devices] = {};
int VulkanStorage::g_instance_ref_count = 0;

Ex<VulkanInstance *> VulkanStorage::allocInstance() {
	if(g_instance_ref_count > 0)
		return ERROR("VulkanInstance already created");
	return new(&g_instance) VulkanInstance;
}

Ex<VulkanDevice *> VulkanStorage::allocDevice(VInstanceRef instance_ref,
											  VPhysicalDeviceId phys_id) {
	for(int i : intRange(max_devices))
		if(g_device_ref_counts[i] == 0)
			return new(&g_devices[i]) VulkanDevice(VDeviceId(i), phys_id, instance_ref);
	return ERROR("Too many VulkanDevices created (max: %)", max_devices);
}

Ex<VWindowRef> VulkanStorage::allocWindow(VInstanceRef instance_ref) {
	int window_id = -1;
	for(int i : intRange(g_windows))
		if(g_windows[i].first == nullptr)
			window_id = i;
	if(window_id == -1) {
		if(g_windows.size() >= max_windows)
			return ERROR("Too many VulkanWindows created (max: %)", max_windows);
		window_id = g_windows.size();
		g_windows.emplace_back(nullptr, 0);
	}

	VWindowId id(window_id);
	auto *window = new VulkanWindow(id, instance_ref);
	g_windows[window_id].first = window;
	return VWindowRef(id, window);
}

void VulkanStorage::incInstanceRef() { g_instance_ref_count++; }
void VulkanStorage::decInstanceRef() {
	if(--g_instance_ref_count == 0)
		reinterpret_cast<VulkanInstance &>(g_instance).~VulkanInstance();
}

void VulkanStorage::incDeviceRef(VDeviceId id) { g_device_ref_counts[id]++; }
void VulkanStorage::decDeviceRef(VDeviceId id) {
	if(--g_device_ref_counts[id] == 0)
		reinterpret_cast<VulkanDevice &>(g_devices[id]).~VulkanDevice();
}

void VulkanStorage::incWindowRef(VWindowId id) { g_windows[id].second++; }
void VulkanStorage::decWindowRef(VWindowId id) {
	if(--g_windows[id].second == 0) {
		auto &ref = g_windows[id].first;
		delete ref;
		ref = nullptr;
	}
}

VInstanceRef::VInstanceRef() { VulkanStorage::incInstanceRef(); }
VInstanceRef::VInstanceRef(const VInstanceRef &) { VulkanStorage::incInstanceRef(); }
VInstanceRef::~VInstanceRef() { VulkanStorage::decInstanceRef(); }
void VInstanceRef::operator=(const VInstanceRef &) {}

VulkanInstance &VInstanceRef::operator*() const {
	return reinterpret_cast<VulkanInstance &>(VulkanStorage::g_instance);
}
VulkanInstance *VInstanceRef::operator->() const {
	return reinterpret_cast<VulkanInstance *>(&VulkanStorage::g_instance);
}

VDeviceRef::VDeviceRef(VDeviceId id) : m_id(id) { VulkanStorage::incDeviceRef(id); }
VDeviceRef::VDeviceRef(const VDeviceRef &rhs) : m_id(rhs.m_id) {
	VulkanStorage::incDeviceRef(m_id);
}
VDeviceRef::~VDeviceRef() { VulkanStorage::decDeviceRef(m_id); }
void VDeviceRef::operator=(const VDeviceRef &rhs) {
	if(m_id != rhs.m_id) {
		VulkanStorage::decDeviceRef(m_id);
		m_id = rhs.m_id;
		VulkanStorage::incDeviceRef(m_id);
	}
}

VWindowRef::VWindowRef(VWindowId id, VulkanWindow *ptr) : m_id(id), m_ptr(ptr) {
	VulkanStorage::incWindowRef(m_id);
}
VWindowRef::VWindowRef(const VWindowRef &rhs) : m_id(rhs.m_id), m_ptr(rhs.m_ptr) {
	VulkanStorage::incWindowRef(m_id);
}
VWindowRef::~VWindowRef() { VulkanStorage::decWindowRef(m_id); }
void VWindowRef::operator=(const VWindowRef &rhs) {
	if(m_id != rhs.m_id) {
		VulkanStorage::decWindowRef(m_id);
		m_id = rhs.m_id;
		m_ptr = rhs.m_ptr;
		VulkanStorage::incWindowRef(m_id);
	}
}
}