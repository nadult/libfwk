// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_memory.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"

#include <vulkan/vulkan.h>

namespace fwk {

VulkanAllocator::VulkanAllocator(VDeviceRef device, VMemoryDomain domain, u64 slab_zone_size)
	: m_device(&*device), m_device_handle(device), m_memory_type(device->info(domain).type_index),
	  m_slabs(slab_zone_size, {&slabAlloc, this}) {}
VulkanAllocator::~VulkanAllocator() = default;

Ex<VulkanAllocator::Allocation> VulkanAllocator::alloc(u64 size, uint alignment) {
	if(alignment > SlabAllocator::min_alignment) {
		// TODO: 256 in most cases is achievable
		EXPECT("TODO: add support for greater alignment");
	}

	auto [identifier, alloc] = m_slabs.alloc(size);
	if(identifier.isValid()) {
		return Allocation{identifier, m_slab_memory[alloc.zone_id], alloc.zone_offset};
	} else {
		// TODO: alignment?
		auto mem = EX_PASS(m_device->allocDeviceMemory(size, m_memory_type));

		int index = -1;
		for(int i : intRange(m_free_memory))
			if(!m_free_memory) {
				m_free_memory[i] = move(mem);
				index = i;
				break;
			}
		if(index == -1) {
			index = m_free_memory.size();
			m_free_memory.emplace_back(move(mem));
		}
		return Allocation{FreeIdentifier{index}, m_free_memory.back(), 0};
	}
}

void VulkanAllocator::free(Identifier ident) {
	ident.match([&](const SlabIdentifier &ident) { m_slabs.free(ident); },
				[&](const FreeIdentifier &ident) { m_free_memory[ident.index] = {}; });
}

u64 VulkanAllocator::slabAlloc(u64 size, uint zone_index, void *ptr) {
	return ((VulkanAllocator *)ptr)->slabAlloc(size, zone_index);
}

u64 VulkanAllocator::slabAlloc(u64 req_size, uint zone_index) {
	auto result = m_device->allocDeviceMemory(req_size, m_memory_type);
	if(!result) {
		m_errors.emplace_back(result.error());
		return 0;
	}
	DASSERT(zone_index == m_slab_memory.size());
	m_slab_memory.emplace_back(move(*result));
	return req_size;
}

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