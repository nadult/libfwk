// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanImage {
  public:
	VulkanImage(VkFormat, VkExtent2D);
	~VulkanImage();

	static Ex<PVImage> create(VDeviceRef, VkFormat, VkExtent2D);
	static Ex<PVImage> createExternal(VDeviceRef, VkImage, VkFormat, VkExtent2D);

	VkExtent2D extent() const { return m_extent; }
	VkFormat format() const { return m_format; }

  private:
	VkExtent2D m_extent;
	VkFormat m_format;
	bool m_is_external = false;
};

class VulkanImageView {
  public:
	VulkanImageView(PVImage, VkFormat, VkExtent2D);
	~VulkanImageView();

	static Ex<PVImageView> create(VDeviceRef, PVImage);

	VkExtent2D extent() const { return m_extent; }
	VkFormat format() const { return m_format; }
	PVImage image() const { return m_image; }

  private:
	PVImage m_image;
	VkExtent2D m_extent;
	VkFormat m_format;
};

}