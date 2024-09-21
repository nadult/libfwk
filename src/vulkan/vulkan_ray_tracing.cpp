// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_ray_tracing.h"

#include "fwk/enum_map.h"
#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_command_queue.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_memory_manager.h"

namespace fwk {

VulkanAccelStruct::VulkanAccelStruct(VkAccelerationStructureKHR handle, VObjectId id,
									 PVBuffer buffer)
	: VulkanObjectBase(handle, id), m_buffer(std::move(buffer)) {}

VulkanAccelStruct::~VulkanAccelStruct() {
	deferredRelease(vkDestroyAccelerationStructureKHR, m_handle);
}

Ex<PVAccelStruct> VulkanAccelStruct::create(VulkanDevice &device, VAccelStructType type,
											VBufferSpan<char> buffer) {
	DASSERT(buffer.byteOffset() % 256 == 0);

	VkAccelerationStructureCreateInfoKHR create_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
	create_info.buffer = buffer.buffer().handle();
	create_info.offset = buffer.byteOffset();
	create_info.size = buffer.byteSize();
	create_info.type = VkAccelerationStructureTypeKHR(type);

	VkAccelerationStructureKHR handle = 0;
	FWK_VK_EXPECT_CALL(vkCreateAccelerationStructureKHR, device.handle(), &create_info, nullptr,
					   &handle);
	return device.createObject(handle, buffer.buffer());
}

static u64 getAddress(VulkanDevice &device, const VulkanBuffer &buffer) {
	VkBufferDeviceAddressInfo info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
	info.buffer = buffer.handle();
	return vkGetBufferDeviceAddress(device.handle(), &info);
}

template <class T> static u64 getAddress(VulkanDevice &device, VBufferSpan<T> buffer) {
	return getAddress(device, *buffer.buffer()) + buffer.byteOffset();
}

Ex<PVAccelStruct> VulkanAccelStruct::build(VulkanDevice &device, VBufferSpan<float3> vertices,
										   VBufferSpan<u32> indices) {
	DASSERT(vertices.buffer()->usage() & VBufferUsage::accel_struct_build_input_read_only);
	DASSERT(indices.buffer()->usage() & VBufferUsage::accel_struct_build_input_read_only);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = getAddress(device, vertices);
	triangles.vertexStride = sizeof(float3);
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = getAddress(device, indices);
	DASSERT(vertices.size() > 0);
	triangles.maxVertex = vertices.size() - 1;

	VkAccelerationStructureGeometryKHR geometry{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles = triangles;

	VkAccelerationStructureBuildRangeInfoKHR build_range_info;
	build_range_info.firstVertex = vertices.byteOffset() / sizeof(float3);
	build_range_info.primitiveCount = indices.size() / 3;
	build_range_info.primitiveOffset = 0;
	build_range_info.transformOffset = 0;
	auto *build_range_infos = &build_range_info;

	VkAccelerationStructureBuildGeometryInfoKHR build_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	build_info.type =
		VkAccelerationStructureTypeKHR::VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	build_info.flags = 0; // TODO: compaction
	build_info.pGeometries = &geometry;
	build_info.geometryCount = 1;

	VkAccelerationStructureBuildSizesInfoKHR size_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	uint32_t prim_count = indices.size() / 3;

	vkGetAccelerationStructureBuildSizesKHR(device.handle(),
											VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
											&build_info, &prim_count, &size_info);

	auto buffer = EX_PASS(VulkanBuffer::create(device, size_info.accelerationStructureSize,
											   VBufferUsage::accel_struct_storage));
	auto accel = EX_PASS(VulkanAccelStruct::create(device, VAccelStructType::bottom_level, buffer));

	accel->m_scratch_buffer =
		EX_PASS(VulkanBuffer::create(device, size_info.buildScratchSize,
									 VBufferUsage::device_address | VBufferUsage::storage_buffer));
	build_info.scratchData.deviceAddress = getAddress(device, *accel->m_scratch_buffer);

	build_info.srcAccelerationStructure = accel->handle();
	build_info.dstAccelerationStructure = accel->handle();

	auto &cmds = device.cmdQueue();
	auto cmd_handle = cmds.bufferHandle();
	vkCmdBuildAccelerationStructuresKHR(cmd_handle, 1, &build_info, &build_range_infos);
	// TODO: barriers here in case of multiple builds
	// TODO: compacting
	// TODO: clear scratch buffer once building is done

	return accel;
}

}