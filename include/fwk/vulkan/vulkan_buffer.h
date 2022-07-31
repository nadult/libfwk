// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanBuffer : public VulkanObjectBase<VulkanBuffer> {
  public:
	static Ex<PVBuffer> create(VulkanDevice &, u64 num_bytes, VBufferUsageFlags,
							   VMemoryUsage = VMemoryUsage::device);
	static Ex<PVBuffer> createAndUpload(VulkanDevice &, CSpan<char>, VBufferUsageFlags,
										VMemoryUsage = VMemoryUsage::device);

	template <class T>
	static Ex<VBufferSpan<T>> create(VulkanDevice &, u32 num_elements, VBufferUsageFlags,
									 VMemoryUsage = VMemoryUsage::device);

	template <c_span TSpan, class T = RemoveConst<SpanBase<TSpan>>>
	static Ex<VBufferSpan<T>> createAndUpload(VulkanDevice &, const TSpan &data, VBufferUsageFlags,
											  VMemoryUsage = VMemoryUsage::device);

	void upload(CSpan<char>);
	template <class T> void upload(CSpan<T> data) { upload(data.template reinterpret<char>()); }
	template <class TSpan, class T = SpanBase<TSpan>> void upload(const TSpan &data) {
		upload<T>(cspan(data));
	}

	u32 size() const { return m_memory_block.size; }
	auto memoryBlock() const { return m_memory_block; }
	auto usage() const { return m_usage; }

	//static Ex<PShaderModule> make(VDeviceId, ShaderType, ZStr code);
	//static Ex<PShaderModule> load(VDeviceId, ZStr file_name);

  private:
	friend class VulkanDevice;
	VulkanBuffer(VkBuffer, VObjectId, VMemoryBlock, VBufferUsageFlags);
	~VulkanBuffer();

	VMemoryBlock m_memory_block;
	VBufferUsageFlags m_usage;
};
}