// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

// TODO: make objects immovable
class VulkanBuffer : public VulkanObjectBase<VulkanBuffer> {
  public:
	static Ex<PVBuffer> create(VDeviceRef, u64 num_bytes, VBufferUsageFlags usage);
	template <class T>
	static Ex<PVBuffer> create(VDeviceRef device, u64 num_elements, VBufferUsageFlags usage) {
		return create(device, num_elements * sizeof(T), usage);
	}

	VkMemoryRequirements memoryRequirements() const;
	Ex<void> bindMemory(PVDeviceMemory);

	void upload(CSpan<char>);
	template <class T> void upload(CSpan<T> data) { upload(data.template reinterpret<char>()); }
	template <class TSpan, class T = SpanBase<TSpan>> void upload(const TSpan &data) {
		upload<T>(cspan(data));
	}

	u64 size() const { return m_size; }
	auto usage() const { return m_usage; }
	auto memoryFlags() const { return m_mem_flags; }

	//static Ex<PShaderModule> make(VDeviceId, ShaderType, ZStr code);
	//static Ex<PShaderModule> load(VDeviceId, ZStr file_name);

  private:
	friend class VulkanDevice;
	VulkanBuffer(VkBuffer, VObjectId, u64 size, VBufferUsageFlags);
	~VulkanBuffer();

	u64 m_size;
	PVDeviceMemory m_memory;
	VMemoryFlags m_mem_flags;
	VBufferUsageFlags m_usage;
};

}