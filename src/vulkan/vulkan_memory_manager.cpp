// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_memory_manager.h"

#include "fwk/slab_allocator.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"

#include <vulkan/vulkan.h>

namespace fwk {

VulkanMemoryManager::VulkanMemoryManager(VDeviceRef device)
	: m_device(&*device), m_device_handle(device) {
	auto &phys_info = VulkanInstance::ref()->info(device->physId());
	auto &info = phys_info.mem_properties;
	EnumMap<VMemoryDomain, VMemoryFlags> domain_flags = {
		{VMemoryFlag::device_local, VMemoryFlag::host_visible,
		 VMemoryFlag::device_local | VMemoryFlag::host_visible}};

	for(auto domain : all<VMemoryDomain>) {
		int type_index = phys_info.findMemoryType(~0u, domain_flags[domain]);
		int heap_index = -1;
		u64 heap_size = 0;
		if(type_index != -1) {
			heap_index = info.memoryTypes[type_index].heapIndex;
			heap_size = info.memoryHeaps[heap_index].size;
		}

		auto &info = m_domains[domain];
		info.domain = domain;
		info.device = m_device;
		info.type_index = type_index;
		info.heap_index = heap_index;
		info.heap_size = heap_size;
	}

	bool temp_available = isAvailable(VMemoryDomain::temporary);
	m_frame_allocator_domain = temp_available ? VMemoryDomain::temporary : VMemoryDomain::host;
}
VulkanMemoryManager::~VulkanMemoryManager() {
	// TODO: free
}

void VulkanMemoryManager::beginFrame() {
	if(!m_is_initial_frame) {
		m_frame_index = (m_frame_index + 1) % num_frames;
		for(auto ident : m_deferred_frees[m_frame_index])
			immediateFree(ident);
		m_deferred_frees[m_frame_index].clear();
	}
	m_is_initial_frame = false;
	m_frames[m_frame_index].offset = 0;
}

void VulkanMemoryManager::addSlabAllocator(VMemoryDomain domain, u64 zone_size) {
	auto &info = m_domains[domain];
	info.slab_zone_size = zone_size;
	if(!info.slab_alloc) {
		SlabAllocator::ZoneAllocator zone_alloc{&slabAlloc, &info};
		info.slab_alloc.emplace(zone_size, zone_alloc);
	}
}

auto VulkanMemoryManager::unmanagedAlloc(VMemoryDomain domain_id, u64 size, uint alignment)
	-> Ex<Allocation> {
	// TODO: alignment?
	auto &domain = m_domains[domain_id];
	auto mem = EX_PASS(m_device->allocDeviceMemory(size, domain.type_index));

	int index = -1;
	for(int i : intRange(domain.free_memory))
		if(!domain.free_memory) {
			domain.free_memory[i] = move(mem);
			index = i;
			break;
		}
	if(index == -1) {
		index = domain.free_memory.size();
		domain.free_memory.emplace_back(move(mem));
	}
	AllocId alloc_id(VAllocationType::unmanaged, domain_id, index);
	return Allocation{alloc_id, domain.free_memory.back(), 0};
}

auto VulkanMemoryManager::alloc(VMemoryDomain domain_id, u64 size, uint alignment)
	-> Ex<Allocation> {
	auto &domain = m_domains[domain_id];

	if(alignment > SlabAllocator::min_alignment) {
		// TODO: 256 in most cases is achievable
		EXPECT("TODO: add support for greater alignment");
	}

	if(domain.slab_alloc) {
		auto [ident, alloc] = domain.slab_alloc->alloc(size);
		if(ident.isValid())
			return Allocation{{VAllocationType::slab, domain_id, ident.value},
							  domain.slab_memory[alloc.zone_id],
							  alloc.zone_offset};
	}

	return unmanagedAlloc(domain_id, size, alignment);
}

void VulkanMemoryManager::immediateFree(AllocId ident) {
	switch(ident.type) {
	case VAllocationType::unmanaged:
		m_domains[ident.domain].free_memory[ident.block_id] = {};
		break;
	case VAllocationType::slab: {
		SlabAllocator::Identifier slab_ident;
		slab_ident.value = ident.block_id;
		m_domains[ident.domain].slab_alloc->free(slab_ident);
	} break;
	case VAllocationType::frame:
		// Nothing to do here
		break;
	}
}

void VulkanMemoryManager::deferredFree(AllocId ident) {
	if(ident.type == VAllocationType::frame)
		return;
	m_deferred_frees[m_frame_index].emplace_back(ident);
}

u64 VulkanMemoryManager::slabAlloc(u64 size, uint zone_index, void *domain_ptr) {
	DomainInfo &domain = *reinterpret_cast<DomainInfo *>(domain_ptr);
	auto result = domain.device->allocDeviceMemory(size, domain.type_index);
	if(!result)
		return 0;
	DASSERT(zone_index == domain.slab_memory.size());
	domain.slab_memory.emplace_back(move(*result));
	return size;
}

static u64 alignOffset(u64 offset, uint alignment_mask) {
	return (offset + alignment_mask) & (~alignment_mask);
}

auto VulkanMemoryManager::frameAlloc(u64 size, uint alignment) -> Ex<Allocation> {
	auto &frame = m_frames[m_frame_index];
	uint alignment_mask = alignment - 1;

	u64 aligned_offset = alignOffset(frame.offset, alignment_mask);
	if(aligned_offset + size - frame.base_offset > frame.size) {
		auto type_index = m_domains[m_frame_allocator_domain].type_index;
		u64 min_size = aligned_offset - frame.base_offset + size;
		u64 new_size = max(frame.size * 2, m_frame_allocator_base_size, min_size);
		auto new_mem = EX_PASS(alloc(m_frame_allocator_domain, new_size, alignment));
		if(frame.alloc_id)
			deferredFree(*frame.alloc_id);
		frame.alloc_id = new_mem.identifier;
		frame.offset = frame.base_offset = new_mem.offset;
		frame.size = new_size;
		frame.handle = new_mem.handle;
		aligned_offset = frame.offset;
	}

	frame.offset = aligned_offset + size;
	AllocId alloc_id(VAllocationType::frame, m_frame_allocator_domain, 0);
	return Allocation{alloc_id, frame.handle, aligned_offset};
}

Ex<void> VulkanMemoryManager::frameAlloc(PVBuffer buffer) {
	auto buf_handle = buffer.handle();
	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(m_device_handle, buf_handle, &requirements);
	auto type_index = m_domains[m_frame_allocator_domain].type_index;
	if(!(requirements.memoryTypeBits & (1u << type_index)))
		return ERROR("VulkanBuffer requires different memory type (index:% not in mask:%)",
					 type_index, stdFormat("%x", requirements.memoryTypeBits));

	auto allocation = EX_PASS(frameAlloc(requirements.size, requirements.alignment));
	if(vkBindBufferMemory(m_device_handle, buffer, allocation.handle, allocation.offset) !=
	   VK_SUCCESS)
		return ERROR("vkBindBufferMemory failed");
	return {};
}

auto VulkanMemoryManager::budget() const -> EnumMap<VMemoryDomain, Budget> {
	EnumMap<VMemoryDomain, Budget> out;

	if(m_device->features() & VDeviceFeature::memory_budget) {
		VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
		VkPhysicalDeviceMemoryProperties2 props{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
		props.pNext = &budget;
		vkGetPhysicalDeviceMemoryProperties2(m_device->physHandle(), &props);

		for(auto domain : all<VMemoryDomain>) {
			int type_index = m_domains[domain].type_index;
			if(type_index != -1) {
				auto heap_index = m_domains[domain].heap_index;
				out[domain].heap_budget = budget.heapBudget[heap_index];
				out[domain].heap_usage = budget.heapUsage[heap_index];
			}
		}
	}
	return out;
}
}