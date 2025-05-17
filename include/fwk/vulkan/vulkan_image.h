// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

using VFormatVariant = Variant<VColorFormat, VDepthStencilFormat>;

struct VImageDimensions {
	VImageDimensions(int3 size, int num_mip_levels = 1, int num_samples = 1);
	VImageDimensions(int2 size, int num_mip_levels = 1, int num_samples = 1);
	VImageDimensions() : size(0), num_mip_levels(1), num_samples(1) {}

	int3 size;
	i16 num_mip_levels;
	i16 num_samples;
};

// TODO: Add VImageSpan, VImageDimensions -> VImageSize

struct VImageSetup {
	VImageSetup(VColorFormat, VImageDimensions,
				VImageUsageFlags = VImageUsage::transfer_dst | VImageUsage::sampled,
				VImageLayout = VImageLayout::undefined);
	VImageSetup(VDepthStencilFormat, VImageDimensions,
				VImageUsageFlags = VImageUsage::depth_stencil_att,
				VImageLayout = VImageLayout::undefined);
	VImageSetup(VFormatVariant, VImageDimensions, VImageUsageFlags,
				VImageLayout = VImageLayout::undefined);

	VImageDimensions dims;
	VFormatVariant format;
	VImageUsageFlags usage;
	VImageLayout layout;
};

class VulkanImage : public VulkanObjectBase<VulkanImage> {
  public:
	using Layout = VImageLayout;
	static Ex<PVImage> create(VulkanDevice &, const VImageSetup &,
							  VMemoryUsage = VMemoryUsage::device);
	static PVImage createExternal(VulkanDevice &, VkImage, const VImageSetup &);

	static Ex<PVImage> createAndUpload(VulkanDevice &, CSpan<Image>);

	auto memoryBlock() { return m_memory_block; }
	auto dimensions() const { return m_dims; }
	auto size() const { return m_dims.size; }
	const VFormatVariant &format() const { return m_format; }
	auto usage() const { return m_usage; }
	int3 mipSize(int mip_level) const;

	// External image may become invalid (for example when swap chain is destroyed)
	bool isValid() const { return m_is_valid; }

	// ---------- Commands --------------------------------------------------------------

	Ex<> upload(CSpan<Image>, Layout target_layout = Layout::shader_ro);
	Ex<> upload(const Image &, int2 target_offset = {0, 0}, int target_mip = 0,
				Layout target_layout = Layout::shader_ro);

	Layout layout(int mip_level) const;
	void transitionLayout(Layout target_layout, int mip_level = 0);

  private:
	void setLayout(Layout, int mip_level);

	friend class VulkanDevice;
	friend class VulkanSwapChain;
	friend class VulkanCommandQueue;

	VulkanImage(VkImage, VObjectId, VMemoryBlock, const VImageSetup &);
	~VulkanImage();

	VMemoryBlock m_memory_block;
	VFormatVariant m_format;
	VImageDimensions m_dims;
	VImageUsageFlags m_usage;
	u64 m_layout_bits = 0;
	bool m_is_external = false;
	bool m_is_valid = true;
};

class VulkanImageView : public VulkanObjectBase<VulkanImageView> {
  public:
	static PVImageView create(PVImage, Maybe<VImageAspectFlags> aspect = none);

	auto dimensions() const { return m_dims; }
	auto size() const { return m_dims.size; }
	auto size2D() const { return m_dims.size.xy(); }
	const VFormatVariant &format() const { return m_format; }
	PVImage image() const { return m_image; }

  private:
	friend class VulkanDevice;
	VulkanImageView(VkImageView, VObjectId);
	~VulkanImageView();

	PVImage m_image;
	VImageDimensions m_dims;
	VFormatVariant m_format;
};
}