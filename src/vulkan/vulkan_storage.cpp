// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_storage.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_window.h"

#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_render_pass.h"

#include <vulkan/vulkan.h>

namespace fwk {

#if VK_USE_64_BIT_PTR_DEFINES == 0
#error VPtr<> conversion to VkHandle won't work correctly;; TODO: fix it
#endif

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

	int reserve = 16;
	for(auto type_id : all<VTypeId>) {
		auto &cur = objects[type_id];
		cur.counters.reserve(reserve);
		cur.handles.reserve(reserve);
		cur.counters.emplace_back(0u);
		cur.handles.emplace_back(nullptr);
	}
#define CASE_WRAPPED_TYPE(Wrapper, _, type_id)                                                     \
	objects[VTypeId::type_id].wrapped_type_size = sizeof(Wrapper);
#include "fwk/vulkan/vulkan_types.h"
}

VulkanStorage::~VulkanStorage() {}

Ex<VInstanceRef> VulkanStorage::allocInstance() {
	if(instance_ref_count > 0)
		return ERROR("VulkanInstance already created");
	new(&instance) VulkanInstance;
	return VInstanceRef();
}

Ex<VDeviceRef> VulkanStorage::allocDevice(VInstanceRef instance_ref, VPhysicalDeviceId phys_id) {
	for(int i : intRange(max_devices))
		if(device_ref_counts[i] == 0) {
			new(&devices[i]) VulkanDevice(VDeviceId(i), phys_id, instance_ref);
			return VDeviceRef(VDeviceId(i));
		}
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

template <class THandle> void VulkanStorage::decRef(VObjectId id) {
	if(!id)
		return;

	using Wrapper = typename VulkanTypeInfo<THandle>::Wrapper;
	auto type_id = VulkanTypeInfo<THandle>::type_id;

	auto &objects = this->objects[type_id];
	int object_id = id.objectId();
	if(--objects.counters[object_id] == 0) {
		auto *bytes = objects.wrapper_data.data();
		reinterpret_cast<Wrapper *>(bytes)[object_id].~Wrapper();

		auto &tbr_lists = objects.to_be_released_lists[id.deviceId()];
		objects.counters[object_id] = tbr_lists.back();
		tbr_lists.back() = object_id;
	}
}

void VulkanStorage::nextReleasePhase(VDeviceId device_id, VkDevice device_handle) {
	for(auto type_id : all<VTypeId>)
		objects[type_id].nextReleasePhase(type_id, device_id, device_handle);
}

VObjectId VulkanStorage::ObjectStorage::addHandle(VDeviceId device_id, void *handle) {
	int object_id = 0;
	if(free_list != 0) {
		object_id = int(free_list);
		free_list = counters[free_list];
		counters[object_id] = 1u;
		handles[object_id] = handle;
	} else {
		object_id = int(counters.size());
		counters.emplace_back(1u);
		handles.emplace_back(handle);
	}
	return VObjectId(device_id, object_id);
}

template <class THandle>
VObjectId VulkanStorage::ObjectStorage::addObject(VDeviceRef device, THandle handle) {
	auto id = addHandle(device.id(), handle);
	using Wrapper = typename VulkanTypeInfo<THandle>::Wrapper;

	if constexpr(!is_same<Wrapper, None>) {
		int capacity = counters.capacity();
		if(wrapper_data.size() < capacity * sizeof(Wrapper)) {
			counters[id.objectId()]--;
			resizeObjectData<Wrapper>(capacity);
			counters[id.objectId()]++;
		}
	}
	return id;
}

template <class TWrapper> void VulkanStorage::ObjectStorage::resizeObjectData(int new_capacity) {
	int byte_capacity = new_capacity * sizeof(TWrapper);
	if(wrapper_data.size() >= byte_capacity)
		return;
	PodVector<u8> new_data(byte_capacity);

	auto *old_objects = reinterpret_cast<TWrapper *>(wrapper_data.data());
	auto *new_objects = reinterpret_cast<TWrapper *>(new_data.data());

	// Moving objects to new memory range
	for(int i : intRange(counters))
		if(counters[i] != 0) {
			new(&new_objects[i]) TWrapper(move(old_objects[i]));
			old_objects[i].~TWrapper();
		}
	wrapper_data.swap(new_data);
}

// TODO: turn into template ?
void VulkanStorage::ObjectStorage::nextReleasePhase(VTypeId type_id, VDeviceId device_id,
													VkDevice device_handle) {
	auto &tbr_lists = to_be_released_lists[device_id];

	if(tbr_lists.front() != 0) {
		void (*destroy_func)(void *handle, VkDevice device) = nullptr;
#define SIMPLE_FUNC(type_id, type, func_name)                                                      \
	case VTypeId::type_id:                                                                         \
		destroy_func = [](void *handle, VkDevice device) {                                         \
			return func_name(device, (type)handle, nullptr);                                       \
		};                                                                                         \
		break;

		switch(type_id) {
			SIMPLE_FUNC(buffer, VkBuffer, vkDestroyBuffer)
			SIMPLE_FUNC(fence, VkFence, vkDestroyFence)
			SIMPLE_FUNC(semaphore, VkSemaphore, vkDestroySemaphore)
			SIMPLE_FUNC(shader_module, VkShaderModule, vkDestroyShaderModule)
			SIMPLE_FUNC(render_pass, VkRenderPass, vkDestroyRenderPass)
		default:
			FATAL("destroy_func unimplemented for: %s", toString(type_id));
		}

#undef SIMPLE_FUNC

		int object_id = tbr_lists.front();
		while(object_id != 0) {
			destroy_func(handles[object_id], device_handle);
			uint next = counters[object_id];
			handles[object_id] = nullptr;
			counters[object_id] = free_list;
			free_list = object_id;
			object_id = next;
		}
	}
	for(int i = 1; i < tbr_lists.size(); i++)
		tbr_lists[i - 1] = tbr_lists[i];
	tbr_lists.back() = 0;
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

#define CASE_TYPE(_, VkType, __)                                                                   \
	template void VulkanStorage::decRef<VkType>(VObjectId);                                        \
	template VObjectId VulkanStorage::ObjectStorage::addObject<VkType>(VDeviceRef, VkType);
#include "fwk/vulkan/vulkan_types.h"
}