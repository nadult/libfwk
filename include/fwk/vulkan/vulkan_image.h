// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanImage {
  public:
	VulkanImage(VkFormat, int2);
	~VulkanImage();

	static Ex<PVImage> create(VDeviceRef, VkFormat, int2 size);
	static Ex<PVImage> createExternal(VDeviceRef, VkImage, VkFormat, int2 size);

	int2 size() const { return m_size; }
	VkFormat format() const { return m_format; }

  private:
	int2 m_size;
	VkFormat m_format;
	bool m_is_external = false;
};

class VulkanImageView {
  public:
	VulkanImageView(PVImage, VkFormat, int2);
	~VulkanImageView();

	static Ex<PVImageView> create(VDeviceRef, PVImage);

	int2 size() const { return m_size; }
	VkFormat format() const { return m_format; }
	PVImage image() const { return m_image; }

  private:
	PVImage m_image;
	int2 m_size;
	VkFormat m_format;
};

}