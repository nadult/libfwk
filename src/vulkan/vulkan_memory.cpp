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

	auto [chunk, alloc] = m_slabs.alloc(size);
	if(alloc.zone_id >= m_device_mem.size()) {
		DASSERT(alloc.zone_id == m_device_mem.size());
		auto mem = EX_PASS(m_device->allocDeviceMemory(m_slabs.zoneSize(), m_memory_type));
		m_device_mem.emplace_back(move(mem));
	}
	return Allocation{
		.chunk = chunk, .mem_handle = m_device_mem[alloc.zone_id], .mem_offset = alloc.zone_offset};
}

void VulkanAllocator::free(Chunk chunk) { m_slabs.free(chunk); }

VulkanFrameAllocator::VulkanFrameAllocator(VDeviceRef device, u64 base_size)
	: m_device(&*device), m_device_handle(device), m_base_size(base_size) {
	const auto &phys_info = VulkanInstance::ref()->info(device->physId());
	for(auto &pools : m_pools) {
		pools.resize(phys_info.mem_properties.memoryTypeCount);
		for(int i : intRange(pools)) {
			auto props = phys_info.mem_properties.memoryTypes[i].propertyFlags;
			pools[i].flags.bits = props & VMemoryFlags::mask;
		}
	}
}
FWK_MOVABLE_CLASS_IMPL(VulkanFrameAllocator);

void VulkanFrameAllocator::startFrame(int frame_idx) {
	m_frame_idx = frame_idx;
	auto &pools = m_pools[frame_idx];
	for(auto &pool : pools)
		pool.offset = 0;
}

static u64 alignOffset(u64 offset, uint alignment_mask) {
	return (offset + alignment_mask) & (~alignment_mask);
}

Ex<void> VulkanFrameAllocator::alloc(PVBuffer buffer, VMemoryFlags flags) {
	DASSERT("Run startFrame first" && m_frame_idx != -1);

	auto buf_handle = buffer.handle();
	VkMemoryRequirements mem_requirements;
	vkGetBufferMemoryRequirements(m_device_handle, buf_handle, &mem_requirements);

	auto mem_bits = mem_requirements.memoryTypeBits;
	int pool_index = -1;
	auto &pools = m_pools[m_frame_idx];
	for(int i : intRange(m_pools))
		if(((1u << i) & mem_requirements.memoryTypeBits) && (pools[i].flags & flags) == flags) {
			pool_index = i;
			break;
		}

	if(pool_index == -1)
		return ERROR("Couldn't find a suitable pool (bits:% flags:%)",
					 mem_requirements.memoryTypeBits, flags);
	auto &pool = pools[pool_index];
	uint alignment_mask = mem_requirements.alignment - 1;
	uint req_size = mem_requirements.size;

	u64 aligned_offset = alignOffset(pool.offset, alignment_mask);
	if(aligned_offset + req_size > pool.size) {
		u64 new_size = max(pool.size * 2, m_base_size, aligned_offset + req_size);
		auto new_mem = EX_PASS(m_device->allocDeviceMemory(new_size, pool_index));
		pool.size = new_mem->size();
		pool.memory = new_mem;
		pool.offset = 0;
		aligned_offset = 0;
	}

	if(vkBindBufferMemory(m_device_handle, buffer, pool.memory, aligned_offset) != VK_SUCCESS)
		return ERROR("vkBindBufferMemory failed");
	pool.offset = aligned_offset + req_size;
	return {};
}

}