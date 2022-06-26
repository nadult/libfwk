// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

DEFINE_ENUM(VBufferUsageFlag, transfer_src, transfer_dst, uniform_buffer, storage_buffer,
			vertex_buffer, index_buffer, indirect_buffer);
using VBufferUsage = EnumFlags<VBufferUsageFlag>;

// TODO: make objects immovable
// TODO: keep objects in chunks 64-256 objects each;
// Only allocate chunks dynamically, this way objects won't change address
// on allocation
class VulkanBuffer : public VulkanObjectBase<VulkanBuffer> {
  public:
	static Ex<PVBuffer> create(VDeviceRef, u64 num_bytes, VBufferUsage usage);
	template <class T>
	static Ex<PVBuffer> create(VDeviceRef device, u64 num_elements, VBufferUsage usage) {
		return create(device, num_elements * sizeof(T), usage);
	}

	void upload(CSpan<char>);
	template <class T> void upload(CSpan<T> data) { upload(data.template reinterpret<char>()); }
	template <class TSpan, class T = SpanBase<TSpan>> void upload(const TSpan &data) {
		upload<T>(cspan(data));
	}

	//static Ex<PShaderModule> make(VDeviceId, ShaderType, ZStr code);
	//static Ex<PShaderModule> load(VDeviceId, ZStr file_name);

  private:
	friend class VulkanDevice;
	VulkanBuffer(VkBuffer, VObjectId, u64 size, VBufferUsage);
	~VulkanBuffer();

	u64 m_size;
	VkDevice m_device = nullptr; // TODO: shoudln't be needed
	VkDeviceMemory m_memory = nullptr;
	VBufferUsage m_usage;
};

}