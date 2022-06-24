// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan_base.h"

namespace fwk {

class VulkanBuffer {
  public:
	VulkanBuffer(u64 size);
	~VulkanBuffer();

	static Ex<PVBuffer> create(VDeviceRef, u64 num_bytes);
	template <class T> static Ex<PVBuffer> create(VDeviceRef device, u64 num_elements) {
		return create(device, num_elements * sizeof(T));
	}

	void upload(CSpan<char>);
	template <class T> void upload(CSpan<T> data) { upload(data.template reinterpret<char>()); }
	template <class TSpan, class T = SpanBase<TSpan>> void upload(const TSpan &data) {
		upload<T>(cspan(data));
	}

	//static Ex<PShaderModule> make(VDeviceId, ShaderType, ZStr code);
	//static Ex<PShaderModule> load(VDeviceId, ZStr file_name);

  private:
	u64 m_size;
	VkDevice m_device = nullptr; // TODO: shoudln't be needed
	VkDeviceMemory m_memory = nullptr;
};

}