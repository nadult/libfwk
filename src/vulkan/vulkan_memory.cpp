// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_memory.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"

#include <vulkan/vulkan.h>

namespace fwk {

VulkanAllocator::VulkanAllocator(VDeviceRef device, VMemoryDomain domain)
	: m_device(&*device), m_device_handle(device), m_memory_type(device->info(domain).type_index) {}
VulkanAllocator::~VulkanAllocator() = default;

Ex<VulkanAllocator::Allocation> VulkanAllocator::alloc(u64 size, uint alignment) {
	if(alignment > SlabAllocator::min_alignment) {
		// TODO: 256 in most cases is achievable
		EXPECT("TODO: add support for greater alignment");
	}

	auto [identifier, alloc] = m_slabs.alloc(size);
	if(alloc.zone_id >= m_device_mem.size()) {
		DASSERT(alloc.zone_id == m_device_mem.size());
		auto zone_size = m_slabs.zones()[alloc.zone_id].size;
		auto mem = EX_PASS(m_device->allocDeviceMemory(zone_size, m_memory_type));
		m_device_mem.emplace_back(move(mem));
	}
	return Allocation{identifier, m_device_mem[alloc.zone_id], alloc.zone_offset};
}

void VulkanAllocator::free(Identifier ident) { m_slabs.free(ident); }

VulkanFrameAllocator::VulkanFrameAllocator(VDeviceRef device, VMemoryDomain domain, u64 base_size)
	: m_device(&*device), m_device_handle(device), m_base_size(base_size) {
	const auto &phys_info = VulkanInstance::ref()->info(device->physId());
	auto info = device->info(domain);
	ASSERT(info.type_index != -1);
	m_memory_type_index = info.type_index;
}
FWK_MOVABLE_CLASS_IMPL(VulkanFrameAllocator);

void VulkanFrameAllocator::startFrame(int frame_idx) {
	m_frame_idx = frame_idx;
	m_pools[frame_idx].offset = 0;
}

static u64 alignOffset(u64 offset, uint alignment_mask) {
	return (offset + alignment_mask) & (~alignment_mask);
}

auto VulkanFrameAllocator::alloc(u64 req_size, uint req_alignment) -> Ex<Allocation> {
	auto &pool = m_pools[m_frame_idx];
	uint alignment_mask = req_alignment - 1;

	u64 aligned_offset = alignOffset(pool.offset, alignment_mask);
	if(aligned_offset + req_size > pool.size) {
		u64 new_size = max(pool.size * 2, m_base_size, aligned_offset + req_size);
		auto new_mem = EX_PASS(m_device->allocDeviceMemory(new_size, m_memory_type_index));
		pool.size = new_mem->size();
		pool.memory = new_mem;
		pool.offset = 0;
		aligned_offset = 0;
	}

	pool.offset = aligned_offset + req_size;
	return Allocation{pool.memory, aligned_offset};
}

Ex<void> VulkanFrameAllocator::alloc(PVBuffer buffer) {
	DASSERT("Run startFrame first" && m_frame_idx != -1);

	auto buf_handle = buffer.handle();
	VkMemoryRequirements mem_requirements;
	vkGetBufferMemoryRequirements(m_device_handle, buf_handle, &mem_requirements);
	if(!(mem_requirements.memoryTypeBits & (1u << m_memory_type_index)))
		return ERROR("VulkanBuffer requires different memory type (index:% not in mask:%)",
					 m_memory_type_index, stdFormat("%x", mem_requirements.memoryTypeBits));

	auto allocation = EX_PASS(alloc(mem_requirements.size, mem_requirements.alignment));
	if(vkBindBufferMemory(m_device_handle, buffer, allocation.memory, allocation.offset) !=
	   VK_SUCCESS)
		return ERROR("vkBindBufferMemory failed");
	return {};
}
}