// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_swap_chain.h"

#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_framebuffer.h"
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
	uint count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_handle, surf_handle, &count, nullptr);
	out.formats.resize(count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_handle, surf_handle, &count, out.formats.data());

	count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_handle, surf_handle, &count, nullptr);
	out.present_modes.resize(count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_handle, surf_handle, &count,
											  out.present_modes.data());
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
	auto out = device->createObject(VkSwapchainKHR(nullptr), window, present_queue->handle);
	out->m_setup = setup;
	EXPECT(out->initialize());
	return out;
}

Ex<void> VulkanSwapChain::initialize() {
	m_status = Status::invalid;

	auto device = deviceRef();
	auto surf_info = surfaceInfo(device, m_window);
	auto color_usage = flag(VImageUsage::color_att);
	auto color_layout = VImageLayout::color_att;

	if(surf_info.capabilities.currentExtent.width == 0 ||
	   surf_info.capabilities.currentExtent.height == 0)
		return ERROR("Window minimized"); // TODO: this is not really an error

	VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	ci.surface = m_window->surfaceHandle();
	ci.minImageCount = surf_info.capabilities.minImageCount;
	if(ci.minImageCount == 1)
		ci.minImageCount = min(ci.minImageCount + 1, surf_info.capabilities.maxImageCount);
	ci.imageFormat = surf_info.formats[0].format;
	ci.imageColorSpace = surf_info.formats[0].colorSpace;
	ci.imageExtent = surf_info.capabilities.currentExtent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = toVk(color_usage);
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
	m_extent = int2(ci.imageExtent.width, ci.imageExtent.height);

	FWK_VK_EXPECT_CALL(vkCreateSwapchainKHR, device, &ci, nullptr, &m_handle);

	vector<VkImage> images;
	uint num_images = 0;
	vkGetSwapchainImagesKHR(device, m_handle, &num_images, nullptr);
	images.resize(num_images);
	vkGetSwapchainImagesKHR(device, m_handle, &num_images, images.data());

	m_images.resize(num_images);
	for(int i : intRange(num_images)) {
		auto &dst = m_images[i];
		VImageSetup setup(ci.imageFormat, fromVk(ci.imageExtent), 1, color_usage, color_layout);
		auto image = VulkanImage::createExternal(device, images[i], setup);
		dst.view = VulkanImageView::create(device, image);
		dst.framebuffer = VulkanFramebuffer::create(device, {dst.view});
		dst.available_sem = createVkSemaphore(device);
	}

	m_status = Status::initialized;
	return {};
}

void VulkanSwapChain::release() {
	deferredRelease<vkDestroySwapchainKHR>(m_handle);
	for(auto &image : m_images) {
		image.view = {};
		image.framebuffer = {};
		deferredRelease<vkDestroySemaphore>(image.available_sem);
	}
	m_images.clear();
	m_handle = nullptr;
}

bool VulkanSwapChain::isValid() const { return m_handle != nullptr && m_status != Status::invalid; }

Ex<void> VulkanSwapChain::recreate() {
	if(m_handle)
		release();
	deviceRef()->waitForIdle();
	return initialize();
}

bool VulkanSwapChain::acquireImage() {
	DASSERT(isOneOf(m_status, Status::image_presented, Status::initialized));

	int next_index = m_status == Status::initialized ? 0 : (m_image_index + 1) % m_images.size();
	auto &available_sem = m_images[next_index].available_sem;
	uint image_index;
	auto result = vkAcquireNextImageKHR(deviceHandle(), m_handle, UINT64_MAX, available_sem,
										VK_NULL_HANDLE, &image_index);
	if(result != VK_SUCCESS) {
		m_status = Status::invalid;
		return false;
	}

	if(image_index != next_index)
		swap(available_sem, m_images[image_index].available_sem);
	m_image_index = image_index;
	m_status = Status::image_acquired;
	return true;
}

const VulkanSwapChain::ImageInfo &VulkanSwapChain::acquiredImage() const {
	DASSERT_EQ(m_status, Status::image_acquired);
	return m_images[m_image_index];
}

bool VulkanSwapChain::presentImage(VkSemaphore wait_for_sem) {
	DASSERT_EQ(m_status, Status::image_acquired);

	VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &wait_for_sem;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &m_handle;
	present_info.pImageIndices = &m_image_index;

	auto present_result = vkQueuePresentKHR(m_present_queue, &present_info);
	if(present_result == VK_ERROR_OUT_OF_DATE_KHR) {
		m_status = Status::invalid;
		return false;
	}
	m_status = Status::image_presented;
	return true;
}
}