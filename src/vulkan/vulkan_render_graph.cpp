// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_render_graph.h"

#include "fwk/gfx/image.h"
#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_framebuffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_memory_manager.h"
#include "fwk/vulkan/vulkan_pipeline.h"

namespace fwk {

VSpan::VSpan(PVBuffer buffer) : buffer(buffer), offset(0), size(buffer->size()) {}
VSpan::VSpan(PVBuffer buffer, u32 offset)
	: buffer(buffer), offset(offset), size(buffer->size() - offset) {
	PASSERT(offset < buffer->size());
}

VulkanRenderGraph::VulkanRenderGraph(VDeviceRef device)
	: m_device(*device), m_device_handle(device) {}

VulkanRenderGraph::~VulkanRenderGraph() {
	if(!m_device_handle)
		return;
	vkDeviceWaitIdle(m_device_handle);
	for(auto &frame : m_frames) {
		vkDestroySemaphore(m_device_handle, frame.render_finished_sem, nullptr);
		vkDestroyFence(m_device_handle, frame.in_flight_fence, nullptr);
	}
	vkDestroyCommandPool(m_device_handle, m_command_pool, nullptr);
}

Ex<void> VulkanRenderGraph::initialize(VDeviceRef device) {
	auto queue = device->findFirstQueue(VQueueCap::graphics);
	EXPECT(queue);
	m_queue = *queue;

	auto pool_flags = VCommandPoolFlag::reset_command | VCommandPoolFlag::transient;
	m_command_pool = createVkCommandPool(device, m_queue.family_id, pool_flags);
	for(auto &frame : m_frames) {
		frame.command_buffer = allocVkCommandBuffer(device, m_command_pool);
		frame.render_finished_sem = createVkSemaphore(device);
		frame.in_flight_fence = createVkFence(device, true);
	}

	return {};
}

void VulkanRenderGraph::beginFrame() {
	DASSERT(m_status != Status::frame_running);
	auto &frame = m_frames[m_swap_index];

	VkFence fences[] = {frame.in_flight_fence};
	vkWaitForFences(m_device_handle, 1, fences, VK_TRUE, UINT64_MAX);
	vkResetFences(m_device_handle, 1, fences);

	for(auto &download : m_downloads)
		if(download.frame_index + VulkanLimits::num_swap_frames >= m_frame_index)
			download.is_ready = true;

	vkResetCommandBuffer(frame.command_buffer, 0);
	VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	FWK_VK_CALL(vkBeginCommandBuffer, frame.command_buffer, &bi);
	m_cur_cmd_buffer = frame.command_buffer;
	m_status = Status::frame_running;

	// Flushing copy commands
	for(auto &copy_command : m_copy_queue) {
		if(const CmdCopyBuffer *cmd = copy_command)
			copy(cmd->dst, cmd->src);
		else if(const CmdCopyImage *cmd = copy_command)
			copy(cmd->dst, cmd->src, cmd->dst_layout);
	}
	m_copy_queue.clear();
}

void VulkanRenderGraph::finishFrame(VkSemaphore *wait_sem, VkSemaphore *out_signal_sem) {
	DASSERT(m_status == Status::frame_running);
	m_status = Status::frame_finished;

	auto &frame = m_frames[m_swap_index];
	FWK_VK_CALL(vkEndCommandBuffer, frame.command_buffer);

	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &frame.command_buffer;
	if(wait_sem) {
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_sem;
	}
	if(out_signal_sem) {
		*out_signal_sem = frame.render_finished_sem;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = out_signal_sem;
	}

	FWK_VK_CALL(vkQueueSubmit, m_queue.handle, 1, &submit_info, frame.in_flight_fence);
	m_swap_index = (m_swap_index + 1) % VulkanLimits::num_swap_frames;
	m_frame_index++;

	// TODO: staging buffers destruction should be hooked to fence
	m_staging_buffers.clear();
	m_last_pipeline_layout.reset();
	m_last_bind_point = VBindPoint::graphics;
	m_last_viewport = {};
}

bool VulkanRenderGraph::isFinished(VDownloadId download_id) {
	PASSERT(m_downloads.valid(download_id));
	auto &download = m_downloads[download_id];
	return download.is_ready;
}

PodVector<char> VulkanRenderGraph::retrieve(VDownloadId download_id) {
	PASSERT(m_downloads.valid(download_id));
	auto &download = m_downloads[download_id];
	if(!download.is_ready)
		return {};
	auto mem_block = download.buffer->memoryBlock();
	PodVector<char> out_data(download.buffer->size());
	auto src_memory = m_device.memory().accessMemory(mem_block);
	fwk::copy(out_data, src_memory.subSpan(0, out_data.size()));
	m_downloads.erase(download_id);
	return out_data;
}

// -------------------------------------------------------------------------------------------
// ----------  Commands ----------------------------------------------------------------------

void VulkanRenderGraph::copy(VSpan dst, VSpan src) {
	DASSERT(src.size <= dst.size);
	VkBufferCopy copy_params{src.offset, dst.offset, VkDeviceSize(src.size)};
	vkCmdCopyBuffer(m_cur_cmd_buffer, src.buffer, dst.buffer, 1, &copy_params);
}

static void transitionImageLayout(VkCommandBuffer cmd_buffer, VkImage image, VkFormat format,
								  VImageLayout old_layout, VImageLayout new_layout) {
	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.oldLayout = toVk(old_layout);
	barrier.newLayout = toVk(new_layout);
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	VkPipelineStageFlags src_stage, dst_stage;
	// TODO: properly handle access masks & stages
	if(old_layout == VImageLayout::undefined && new_layout == VImageLayout::transfer_dst) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if(old_layout == VImageLayout::transfer_dst && new_layout == VImageLayout::shader_ro) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		FATAL("Unsupported layout transition: %s -> %s", toString(old_layout),
			  toString(new_layout));
	}

	vkCmdPipelineBarrier(cmd_buffer, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRenderGraph::copy(PVImage dst, VSpan src, VImageLayout dst_layout) {
	VkBufferImageCopy region{};
	region.bufferOffset = src.offset;
	// TODO: do it properly
	auto dst_extent = dst->extent();
	region.bufferImageHeight = 0;
	region.bufferRowLength = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageExtent = {uint(dst_extent.x), uint(dst_extent.y), 1};

	auto copy_layout = VImageLayout::transfer_dst;
	transitionImageLayout(m_cur_cmd_buffer, dst, dst->format(), dst->m_last_layout, copy_layout);
	vkCmdCopyBufferToImage(m_cur_cmd_buffer, src.buffer, dst, toVk(copy_layout), 1, &region);

	// TODO: do this transition right before we're going to do something with this texture?
	transitionImageLayout(m_cur_cmd_buffer, dst, dst->format(), copy_layout, dst_layout);
	dst->m_last_layout = dst_layout;
}

void VulkanRenderGraph::bind(PVPipelineLayout layout, VBindPoint bind_point) {
	m_last_pipeline_layout = layout;
	m_last_bind_point = bind_point;
}

void VulkanRenderGraph::bindDS(int index, const VDescriptorSet &ds) {
	vkCmdBindDescriptorSets(m_cur_cmd_buffer, toVk(m_last_bind_point), m_last_pipeline_layout,
							uint(index), 1, &ds.handle, 0, nullptr);
}

VDescriptorSet VulkanRenderGraph::bindDS(int index) {
	DASSERT(m_last_pipeline_layout);
	auto dsl_id = m_last_pipeline_layout->descriptorSetLayouts()[index];
	auto ds = m_device.acquireSet(dsl_id);
	bindDS(index, ds);
	return ds;
}

Ex<VDownloadId> VulkanRenderGraph::download(VSpan src) {
	PASSERT(m_status == Status::frame_running);
	auto buffer = EX_PASS(VulkanBuffer::create(m_device.ref(), src.size, VBufferUsage::transfer_dst,
											   VMemoryUsage::host));
	VkBufferCopy copy_params{
		.srcOffset = src.offset, .dstOffset = 0, .size = (VkDeviceSize)src.size};
	copy(buffer, src);
	auto id = m_downloads.emplace(move(buffer), m_frame_index);
	return VDownloadId(id);
}

void VulkanRenderGraph::setViewport(IRect viewport, float min_depth, float max_depth) {
	PASSERT(m_status == Status::frame_running);
	VkViewport vk_viewport{float(viewport.x()),		 float(viewport.y()), float(viewport.width()),
						   float(viewport.height()), min_depth,			  max_depth};
	m_last_viewport = viewport;
	vkCmdSetViewport(m_cur_cmd_buffer, 0, 1, &vk_viewport);
}

void VulkanRenderGraph::setScissor(Maybe<IRect> scissor) {
	PASSERT(m_status == Status::frame_running);
	DASSERT(scissor || !m_last_viewport.empty());
	IRect rect = scissor.orElse(m_last_viewport);
	VkRect2D vk_rect{{rect.x(), rect.y()}, {uint(rect.width()), uint(rect.height())}};
	vkCmdSetScissor(m_cur_cmd_buffer, 0, 1, &vk_rect);
}

void VulkanRenderGraph::bindIndices(VSpan span) {
	PASSERT(m_status == Status::frame_running);
	vkCmdBindIndexBuffer(m_cur_cmd_buffer, span.buffer, span.offset,
						 VkIndexType::VK_INDEX_TYPE_UINT32);
}

void VulkanRenderGraph::bindVertices(CSpan<VSpan> vbuffers, uint first_binding) {
	PASSERT(m_status == Status::frame_running);
	static constexpr int max_bindings = 32;
	VkBuffer buffers[max_bindings];
	u64 offsets_64[max_bindings];

	DASSERT(vbuffers.size() <= max_bindings);
	for(int i : intRange(vbuffers)) {
		buffers[i] = vbuffers[i].buffer;
		offsets_64[i] = vbuffers[i].offset;
	}
	vkCmdBindVertexBuffers(m_cur_cmd_buffer, first_binding, vbuffers.size(), buffers, offsets_64);
}

void VulkanRenderGraph::bind(PVPipeline pipeline) {
	PASSERT(m_status == Status::frame_running);
	vkCmdBindPipeline(m_cur_cmd_buffer, toVk(pipeline->bindPoint()), pipeline);
}

void VulkanRenderGraph::draw(int num_vertices, int num_instances, int first_vertex,
							 int first_instance) {
	PASSERT(m_status == Status::frame_running);
	vkCmdDraw(m_cur_cmd_buffer, num_vertices, num_instances, first_vertex, first_instance);
}

void VulkanRenderGraph::drawIndexed(int num_indices, int num_instances, int first_index,
									int first_instance, int vertex_offset) {
	PASSERT(m_status == Status::frame_running);
	vkCmdDrawIndexed(m_cur_cmd_buffer, num_indices, num_instances, first_index, vertex_offset,
					 first_instance);
}

// TODO: add cache for renderpasses, add renderpass for framebuffer, interface for clear values
void VulkanRenderGraph::beginRenderPass(PVFramebuffer framebuffer, PVRenderPass render_pass,
										Maybe<IRect> render_area,
										CSpan<VkClearValue> clear_values) {
	PASSERT(m_status == Status::frame_running);
	VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	bi.renderPass = render_pass;
	if(render_area)
		bi.renderArea = toVkRect(*render_area);
	else
		bi.renderArea.extent = toVkExtent(framebuffer->extent());
	bi.clearValueCount = clear_values.size();
	bi.pClearValues = clear_values.data();
	bi.framebuffer = framebuffer;
	vkCmdBeginRenderPass(m_cur_cmd_buffer, &bi, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderGraph::endRenderPass() {
	PASSERT(m_status == Status::frame_running);
	vkCmdEndRenderPass(m_cur_cmd_buffer);
}

void VulkanRenderGraph::dispatchCompute(int3 size) {
	PASSERT(m_status == Status::frame_running);
	vkCmdDispatch(m_cur_cmd_buffer, size.x, size.y, size.z);
}

// -------------------------------------------------------------------------------------------
// ----------  Upload commands ---------------------------------------------------------------

Ex<VSpan> VulkanRenderGraph::upload(VSpan dst, CSpan<char> src) {
	if(src.empty())
		return dst;
	DASSERT(src.size() <= dst.size);

	auto &mem_mgr = m_device.memory();
	auto mem_block = dst.buffer->memoryBlock();
	if(canBeMapped(mem_block.id.domain())) {
		mem_block.offset += dst.offset;
		DASSERT(cmd.offset <= mem_block.size);
		// TODO: better checks
		mem_block.size = min<u32>(mem_block.size - dst.offset, src.size());
		fwk::copy(mem_mgr.accessMemory(mem_block), src);
	} else {
		auto staging_buffer = EX_PASS(VulkanBuffer::create(
			m_device.ref(), src.size(), VBufferUsage::transfer_src, VMemoryUsage::host));
		auto mem_block = staging_buffer->memoryBlock();
		fwk::copy(mem_mgr.accessMemory(mem_block), src);

		if(m_status == Status::frame_running)
			copy(dst, staging_buffer);
		else
			m_copy_queue.emplace_back(CmdCopyBuffer{dst, staging_buffer});
		m_staging_buffers.emplace_back(move(staging_buffer));
	}

	return VSpan{dst.buffer, dst.offset + src.size(), dst.size - src.size()};
}

Ex<void> VulkanRenderGraph::upload(PVImage dst, const Image &src, VImageLayout dst_layout) {
	if(src.empty())
		return {};

	int data_size = src.data().size() * sizeof(IColor);

	auto &mem_mgr = m_device.memory();
	auto mem_block = dst->memoryBlock();
	if(canBeMapped(mem_block.id.domain())) {
		// TODO: better checks
		mem_block.size = min<u32>(mem_block.size, data_size);
		fwk::copy(mem_mgr.accessMemory(mem_block), src.data().reinterpret<char>());
	} else {
		auto staging_buffer = EX_PASS(VulkanBuffer::create(
			m_device.ref(), data_size, VBufferUsage::transfer_src, VMemoryUsage::host));
		auto mem_block = staging_buffer->memoryBlock();
		fwk::copy(mem_mgr.accessMemory(mem_block), src.data().reinterpret<char>());
		if(m_status == Status::frame_running)
			copy(dst, staging_buffer, dst_layout);
		else
			m_copy_queue.emplace_back(CmdCopyImage{dst, staging_buffer, dst_layout});
		m_staging_buffers.emplace_back(move(staging_buffer));
	}

	return {};
}

}