// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_memory_manager.h"

#include "fwk/slab_allocator.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_internal.h"

namespace fwk {

static Ex<VkDeviceMemory> allocDeviceMemory(VkDevice device_handle, u32 size, uint type_index) {
	VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	ai.allocationSize = size;
	ai.memoryTypeIndex = type_index;
	VkDeviceMemory handle;
	FWK_VK_EXPECT_CALL(vkAllocateMemory, device_handle, &ai, nullptr, &handle);
	return handle;
}

static void freeDeviceMemory(VkDevice device_handle, VkDeviceMemory handle) {
	if(handle)
		vkFreeMemory(device_handle, handle, nullptr);
}

VulkanMemoryManager::VulkanMemoryManager(VkDevice device_handle,
										 const VulkanPhysicalDeviceInfo &phys_info,
										 VDeviceFeatures features)
	: m_device_handle(device_handle), m_phys_handle(phys_info.handle),
	  m_has_mem_budget(features & VDeviceFeature::memory_budget) {
	m_non_coherent_atom_size = phys_info.properties.limits.nonCoherentAtomSize;

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
		info.device_handle = m_device_handle;
		info.type_index = type_index;
		info.heap_index = heap_index;
		info.heap_size = heap_size;
	}

	addSlabAllocator(VMemoryDomain::device);
	addSlabAllocator(VMemoryDomain::host);

	bool temp_available = isAvailable(VMemoryDomain::temporary);
	m_frame_allocator_domain = temp_available ? VMemoryDomain::temporary : VMemoryDomain::host;
}

VulkanMemoryManager::~VulkanMemoryManager() {
	for(auto &domain : m_domains) {
		for(auto &mem : domain.slab_memory)
			freeDeviceMemory(m_device_handle, mem.handle);
		for(auto &mem : domain.unmanaged_memory)
			freeDeviceMemory(m_device_handle, mem.handle);
	}
}

void VulkanMemoryManager::freeDeferred(int frame_index) {
	for(auto ident : m_deferred_frees[frame_index])
		immediateFree(ident);
	m_deferred_frees[frame_index].clear();
}

void VulkanMemoryManager::beginFrame() {
	if(!m_is_initial_frame) {
		m_frame_index = (m_frame_index + 1) % num_frames;
		freeDeferred(m_frame_index);
	}
	m_is_initial_frame = false;
	m_frames[m_frame_index].offset = 0;
}

void VulkanMemoryManager::finishFrame() {
	if(m_flush_ranges) {
		// TODO: make sure that all ranges are actually mapped
		// If not, then they should be flushed before unmapping
		auto result = vkFlushMappedMemoryRanges(m_device_handle, m_flush_ranges.size(),
												m_flush_ranges.data());
		// TODO: handle result
		m_flush_ranges.clear();
	};
}

void VulkanMemoryManager::addSlabAllocator(VMemoryDomain domain, u32 zone_size) {
	auto &info = m_domains[domain];
	info.slab_zone_size = zone_size;
	if(!info.slab_alloc) {
		SlabAllocator::ZoneAllocator zone_alloc{&slabAlloc, &info};
		info.slab_alloc.emplace(zone_size, zone_alloc);
	}
}

Ex<VMemoryBlock> VulkanMemoryManager::unmanagedAlloc(VMemoryDomain domain_id, u32 size) {
	auto &domain = m_domains[domain_id];
	auto handle = EX_PASS(allocDeviceMemory(m_device_handle, size, domain.type_index));

	int index = -1;
	for(int i : intRange(domain.unmanaged_memory))
		if(!domain.unmanaged_memory[i].handle) {
			domain.unmanaged_memory[i].handle = handle;
			index = i;
			break;
		}
	if(index == -1) {
		index = domain.unmanaged_memory.size();
		domain.unmanaged_memory.emplace_back(handle, nullptr);
	}
	VMemoryBlockId alloc_id(VMemoryBlockType::unmanaged, domain_id, u16(index), 0);
	return VMemoryBlock{alloc_id, domain.unmanaged_memory.back().handle, 0, size};
}

Ex<VMemoryBlock> VulkanMemoryManager::alloc(VMemoryDomain domain_id, u32 size, uint alignment) {
	auto &domain = m_domains[domain_id];
	if(alignment > SlabAllocator::min_alignment) {
		auto remainder = size % alignment;
		if(remainder != 0)
			size += alignment - remainder;
	}

	if(domain.slab_alloc) {
		auto [ident, alloc] = domain.slab_alloc->alloc(size);
		if(ident.isValid()) {
			VMemoryBlockId block_id(VMemoryBlockType::slab, domain_id, alloc.zone_id, ident.value);
			return VMemoryBlock{block_id, domain.slab_memory[alloc.zone_id].handle,
								u32(alloc.offset), u32(alloc.size)};
		}
	}

	return unmanagedAlloc(domain_id, size);
}

void VulkanMemoryManager::immediateFree(VMemoryBlockId ident) {
	DASSERT(ident.valid());

	switch(ident.type()) {
	case VMemoryBlockType::unmanaged: {
		auto &mem = m_domains[ident.domain()].unmanaged_memory[ident.zoneId()];
		freeDeviceMemory(m_device_handle, mem.handle);
		mem = {};
	} break;
	case VMemoryBlockType::slab: {
		SlabAllocator::Identifier slab_ident;
		slab_ident.value = ident.blockIdentifier();
		m_domains[ident.domain()].slab_alloc->free(slab_ident);
	} break;
	case VMemoryBlockType::frame:
	case VMemoryBlockType::invalid:
		// Nothing to do here
		break;
	}
}

void VulkanMemoryManager::deferredFree(VMemoryBlockId ident) {
	DASSERT(ident.valid());
	if(ident.type() == VMemoryBlockType::frame)
		return;
	m_deferred_frees[m_frame_index].emplace_back(ident);
}

void VulkanMemoryManager::shrunkMappings() {
	// TODO: when is it worth it to do that?
}

static u32 alignOffset(u32 offset, uint alignment_mask) {
	return (offset + alignment_mask) & (~alignment_mask);
}

// TODO: pass errors differently in an uniform way across whole VulkanDevice and members
Span<char> VulkanMemoryManager::accessMemory(const VMemoryBlock &block) {
	DeviceMemory *memory = nullptr;
	auto domain = block.id.domain();
	int zone_id = block.id.zoneId();

	switch(block.id.type()) {
	case VMemoryBlockType::unmanaged:
		memory = &m_domains[domain].unmanaged_memory[zone_id];
		break;
	case VMemoryBlockType::slab:
		memory = &m_domains[domain].slab_memory[zone_id];
		break;
	case VMemoryBlockType::frame:
		memory = &m_frames[zone_id].memory;
		break;
	default:
		return Span<char>();
	}

	if(!memory->mapping)
		FWK_VK_CALL(vkMapMemory, m_device_handle, memory->handle, 0, VK_WHOLE_SIZE, 0,
					&memory->mapping);

	// TODO: handle coherent case
	u32 alignment_mask = m_non_coherent_atom_size - 1;
	u32 offset = block.offset & ~alignment_mask;
	u32 size = ((block.offset - offset) + block.size + alignment_mask) & ~alignment_mask;
	VkMappedMemoryRange mapped_range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
	mapped_range.memory = memory->handle;
	mapped_range.offset = offset;
	mapped_range.size = size;
	m_flush_ranges.emplace_back(mapped_range);

	return Span<char>(((char *)memory->mapping) + block.offset, block.size);
}

u64 VulkanMemoryManager::slabAlloc(u64 size, uint zone_index, void *domain_ptr) {
	if(size > max_allocation_size)
		return 0;
	DomainInfo &domain = *reinterpret_cast<DomainInfo *>(domain_ptr);
	auto result = allocDeviceMemory(domain.device_handle, size, domain.type_index);
	if(!result)
		return 0;
	DASSERT(zone_index == domain.slab_memory.size());
	domain.slab_memory.emplace_back(*result, nullptr);
	return size;
}

auto VulkanMemoryManager::frameAlloc(u32 size, uint alignment) -> Ex<VMemoryBlock> {
	auto &frame = m_frames[m_frame_index];
	uint alignment_mask = alignment - 1;

	u32 aligned_offset = alignOffset(frame.offset, alignment_mask);
	if(aligned_offset + size - frame.base_offset > frame.size) {
		auto type_index = m_domains[m_frame_allocator_domain].type_index;
		u64 min_size = aligned_offset - frame.base_offset + size;
		u64 new_size = max<u64>(frame.size * 2, m_frame_allocator_base_size, min_size);
		new_size = min<u64>(new_size, max_allocation_size);
		EXPECT("Allocation size limit reached for frame allocator" && new_size >= min_size);

		auto new_mem = EX_PASS(alloc(m_frame_allocator_domain, new_size, alignment));
		if(frame.alloc_id)
			deferredFree(*frame.alloc_id);
		frame.alloc_id = new_mem.id;
		frame.offset = frame.base_offset = new_mem.offset;
		frame.size = new_mem.size;
		frame.memory = {new_mem.handle, nullptr};
		aligned_offset = frame.offset;
	}

	frame.offset = aligned_offset + size;
	VMemoryBlockId block_id(VMemoryBlockType::frame, m_frame_allocator_domain, u16(m_frame_index),
							u32(aligned_offset));
	return VMemoryBlock{block_id, frame.memory.handle, aligned_offset, size};
}
bool VulkanMemoryManager::DomainInfo::validDomain(u32 type_mask) const {
	return (type_mask & (1u << type_index)) != 0;
}

Ex<VMemoryBlock> VulkanMemoryManager::alloc(VMemoryUsage usage, const VkMemoryRequirements &reqs) {
	if(usage == VMemoryUsage::frame)
		if(m_domains[m_frame_allocator_domain].validDomain(reqs.memoryTypeBits))
			return frameAlloc(reqs.size, reqs.alignment);

	auto domain = usage == VMemoryUsage::device ? VMemoryDomain::device : VMemoryDomain::host;
	if(m_domains[domain].validDomain(reqs.memoryTypeBits))
		return alloc(domain, reqs.size, reqs.alignment);

	domain = domain == VMemoryDomain::host ? VMemoryDomain::device : VMemoryDomain::host;
	if(m_domains[domain].validDomain(reqs.memoryTypeBits))
		return alloc(domain, reqs.size, reqs.alignment);

	return FWK_ERROR("Couldn't find a memory domain which will satisfy type mask: 0x%",
					 stdFormat("%x", reqs.memoryTypeBits));
}

auto VulkanMemoryManager::budget() const -> EnumMap<VMemoryDomain, Budget> {
	EnumMap<VMemoryDomain, Budget> out;

	if(m_has_mem_budget) {
		VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
		VkPhysicalDeviceMemoryProperties2 props{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
		props.pNext = &budget;
		vkGetPhysicalDeviceMemoryProperties2(m_phys_handle, &props);

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