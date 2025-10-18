// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_storage.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_window.h"

namespace fwk {

#if VK_USE_64_BIT_PTR_DEFINES == 0
#error VPtr<> conversion to VkHandle won't work correctly; TODO: fix it
#endif

VulkanStorage g_vk_storage;

//UndefSize<VulkanInstance> test_size1;
//UndefSize<VulkanDevice> test_size2;
static_assert(sizeof(VulkanInstance) <= sizeof(VulkanStorage::InstanceStorage));
static_assert(alignof(VulkanInstance) <= alignof(VulkanStorage::InstanceStorage));
static_assert(sizeof(VulkanDevice) <= sizeof(VulkanStorage::DeviceStorage));
static_assert(alignof(VulkanDevice) <= alignof(VulkanStorage::DeviceStorage));

VulkanStorage::VulkanStorage() {
	ASSERT("VulkanStorage is a singleton, don't create additional instances" &&
		   this == &g_vk_storage);
}

VulkanStorage::~VulkanStorage() {}

Ex<VInstanceRef> VulkanStorage::allocInstance() {
	if(instance_ref_count > 0)
		return FWK_ERROR("VulkanInstance already created");
	new(&instance) VulkanInstance;
	return VInstanceRef();
}

Ex<VDeviceRef> VulkanStorage::allocDevice(VInstanceRef instance_ref, VPhysicalDeviceId phys_id) {
	for(int i : intRange(max_devices))
		if(device_ref_counts[i] == 0) {
			new(&devices[i]) VulkanDevice(VDeviceId(i), phys_id, instance_ref);
			return VDeviceRef(VDeviceId(i));
		}
	return FWK_ERROR("Too many VulkanDevices created (max: %)", max_devices);
}

Ex<VWindowRef> VulkanStorage::allocWindow(VInstanceRef instance_ref) {
	int window_id = -1;
	for(int i : intRange(windows))
		if(windows[i].first == nullptr)
			window_id = i;
	if(window_id == -1) {
		if(windows.size() >= max_windows)
			return FWK_ERROR("Too many VulkanWindows created (max: %)", max_windows);
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