// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VSurfaceInfo {
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> present_modes;
	bool is_minimized = false;
};

DEFINE_ENUM(VSwapChainStatus, initialized, image_acquired, ready, invalid, window_minimized);

class VulkanSwapChain : public VulkanObjectBase<VulkanSwapChain> {
  public:
	static VSurfaceInfo surfaceInfo(VulkanDevice &, VWindowRef);
	static Ex<PVSwapChain> create(VulkanDevice &, VWindowRef, const VSwapChainSetup &);

	VColorFormat format() const { return m_format; }
	int2 size() const { return m_size; }
	int numImages() const { return m_image_views.size(); }

	using Status = VSwapChainStatus;
	Status status() const { return m_status; }

	// Returns image_available semaphore if image was properly acquired;
	// Returned nullptr means that acquisition failed and user should try again at a later time.
	// Error may also be returned when swap chain recreation failed.
	Ex<VkSemaphore> acquireImage();

	// This function should only be called after successful acquireImage() & before presentImage()
	PVImageView acquiredImage() const;

	// Returns true if image was properly presented; Otherwise returns false which means
	// that user should try again at a later time (starting with acquire). Error may also
	// be returned when swap chain recreation failed.
	Ex<bool> presentImage(VkSemaphore wait_sem);

  private:
	friend class VulkanDevice;
	VulkanSwapChain(VkSwapchainKHR, VObjectId, VWindowRef, VkQueue);
	~VulkanSwapChain();

	Ex<void> initialize(const VSurfaceInfo &);
	Ex<void> recreate();
	void release(bool release_handle);

	VWindowRef m_window;
	VSwapChainSetup m_setup;
	vector<PVImageView> m_image_views;
	array<VkSemaphore, 4> m_semaphores = {};
	VkQueue m_present_queue;
	VColorFormat m_format;
	int2 m_size;
	uint m_image_index = 0;
	uint m_semaphore_index = 0;
	Status m_status = Status::initialized;
};
}