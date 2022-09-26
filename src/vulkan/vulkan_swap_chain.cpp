// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_swap_chain.h"

#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_window.h"

namespace fwk {

VulkanSwapChain::VulkanSwapChain(VkSwapchainKHR handle, VObjectId id, VWindowRef window,
								 VkQueue queue)
	: VulkanObjectBase(handle, id), m_window(window), m_present_queue(queue) {}

VulkanSwapChain::~VulkanSwapChain() { release(); }

VSurfaceInfo VulkanSwapChain::surfaceInfo(VDeviceRef device, VWindowRef window) {
	auto surf_handle = window->surfaceHandle();
	auto phys_handle = device->physHandle();

	VSurfaceInfo out;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_handle, surf_handle, &out.capabilities);

	if(out.capabilities.currentExtent.width == 0 || out.capabilities.currentExtent.height == 0)
		out.is_minimized = true;

	if(!out.is_minimized) {
		uint count = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(phys_handle, surf_handle, &count, nullptr);
		out.formats.resize(count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(phys_handle, surf_handle, &count, out.formats.data());

		count = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(phys_handle, surf_handle, &count, nullptr);
		out.present_modes.resize(count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(phys_handle, surf_handle, &count,
												  out.present_modes.data());
	}

	return out;
}

Ex<PVSwapChain> VulkanSwapChain::create(VDeviceRef device, VWindowRef window,
										const VSwapChainSetup &setup) {
	auto phys_handle = device->physHandle();
	auto surf_handle = window->surfaceHandle();
	Maybe<VQueue> present_queue = none;
	for(auto queue : device->queues()) {
		VkBool32 valid = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(phys_handle, queue.family_id, surf_handle, &valid);
		if(valid) {
			present_queue = queue;
			break;
		}
	}
	if(!present_queue) {
		// Note: it shouldn't be possible if findPresentableQueues() was used during device selection
		return ERROR("VulkanSwapChain: Couldn't find a queue which is presentable");
	}

	auto surf_info = surfaceInfo(device, window);
	if(surf_info.is_minimized)
		return ERROR("Swap chain cannot be created: window is minimized");

	auto out = device->createObject(VkSwapchainKHR(nullptr), window, present_queue->handle);
	out->m_setup = setup;
	EXPECT(out->initialize(surf_info));
	return out;
}

Ex<void> VulkanSwapChain::initialize(const VSurfaceInfo &surf_info) {
	m_status = Status::invalid;
	DASSERT(!surf_info.is_minimized);

	auto device = deviceRef();

	VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	ci.surface = m_window->surfaceHandle();
	ci.minImageCount = surf_info.capabilities.minImageCount;
	if(ci.minImageCount == 1)
		ci.minImageCount = min(ci.minImageCount + 1, surf_info.capabilities.maxImageCount);
	ci.imageFormat = surf_info.formats[0].format;
	ci.imageColorSpace = surf_info.formats[0].colorSpace;
	ci.imageExtent = surf_info.capabilities.currentExtent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = toVk(m_setup.usage);
	ci.preTransform = surf_info.capabilities.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.clipped = VK_TRUE;

	bool valid_preferred = isOneOf(toVk(m_setup.preferred_present_mode), surf_info.present_modes);
	ci.presentMode = toVk(valid_preferred ? m_setup.preferred_present_mode : VPresentMode::fifo);

	bool found_preferred_format = false;
	for(int i = 0; i < m_setup.preferred_formats.size() && !found_preferred_format; i++) {
		auto preferred = m_setup.preferred_formats[i];
		for(auto format : surf_info.formats)
			if(format.format == preferred) {
				ci.imageFormat = format.format;
				ci.imageColorSpace = format.colorSpace;
				break;
			}
	}

	// TODO: handle minimized extents
	m_format = ci.imageFormat;
	m_size = int2(ci.imageExtent.width, ci.imageExtent.height);

	FWK_VK_EXPECT_CALL(vkCreateSwapchainKHR, device, &ci, nullptr, &m_handle);

	vector<VkImage> images;
	uint num_images = 0;
	vkGetSwapchainImagesKHR(device, m_handle, &num_images, nullptr);
	images.resize(num_images);
	vkGetSwapchainImagesKHR(device, m_handle, &num_images, images.data());

	m_image_views.resize(num_images);
	for(int i : intRange(num_images)) {
		VImageSetup setup(ci.imageFormat, fromVk(ci.imageExtent), m_setup.usage,
						  m_setup.initial_layout);
		auto image = VulkanImage::createExternal(device, images[i], setup);
		m_image_views[i] = VulkanImageView::create(device, image);
	}
	for(auto &sem : m_semaphores)
		sem = createVkSemaphore(device);

	m_status = Status::initialized;
	return {};
}

void VulkanSwapChain::release() {
	for(auto &image_view : m_image_views)
		image_view->image()->m_is_valid = false;
	m_image_views.clear();
	for(auto &sem : m_semaphores) {
		deferredRelease<vkDestroySemaphore>(sem);
		sem = nullptr;
	}
	deferredRelease<vkDestroySwapchainKHR>(m_handle);
	m_handle = nullptr;
}

Ex<void> VulkanSwapChain::recreate() {
	if(m_handle) {
		release();
		deviceRef()->waitForIdle();
		m_status = Status::invalid;
	}

	auto surf_info = surfaceInfo(deviceRef(), m_window);
	if(surf_info.is_minimized) {
		m_status = Status::window_minimized;
		return {};
	}
	return initialize(surf_info);
}

Ex<VkSemaphore> VulkanSwapChain::acquireImage() {
	DASSERT(m_status != Status::image_acquired);
	if(isOneOf(m_status, Status::invalid, Status::window_minimized)) {
		EXPECT(recreate());
		if(m_status == Status::window_minimized)
			return nullptr;
	}

	VkResult result;
	for(int retry = 0; retry < 2; retry++) {
		result = vkAcquireNextImageKHR(deviceHandle(), m_handle, UINT64_MAX, m_semaphores[0],
									   VK_NULL_HANDLE, &m_image_index);
		if(result == VK_SUCCESS)
			break;
		EXPECT(recreate());
		if(m_status == Status::window_minimized || retry > 0)
			return nullptr;
	}

	m_status = Status::image_acquired;
	swap(m_semaphores[0], m_semaphores[1]);
	return m_semaphores[1];
}

PVImageView VulkanSwapChain::acquiredImage() const {
	DASSERT_EQ(m_status, Status::image_acquired);
	return m_image_views[m_image_index];
}

Ex<bool> VulkanSwapChain::presentImage(VkSemaphore wait_sem) {
	DASSERT_EQ(m_status, Status::image_acquired);

	VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &wait_sem;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &m_handle;
	present_info.pImageIndices = &m_image_index;

	auto present_result = vkQueuePresentKHR(m_present_queue, &present_info);
	if(present_result == VK_ERROR_OUT_OF_DATE_KHR) {
		EXPECT(recreate());
		if(m_status == Status::window_minimized)
			return false;
	}
	m_status = Status::ready;
	return true;
}
}