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
#include "fwk/vulkan/vulkan_swap_chain.h"

// TODO: what should we do when error happens in begin/end frame?
// TODO: proprly handle multiple queues
// TODO: proprly differentiate between graphics, present & compute queues

namespace fwk {

VulkanRenderGraph::VulkanRenderGraph(VDeviceRef device)
	: m_device(*device), m_device_handle(device) {}

VulkanRenderGraph::~VulkanRenderGraph() {
	if(!m_device_handle)
		return;
	vkDeviceWaitIdle(m_device_handle);
	for(auto &frame : m_frames) {
		vkDestroySemaphore(m_device_handle, frame.image_available_sem, nullptr);
		vkDestroySemaphore(m_device_handle, frame.render_finished_sem, nullptr);
		vkDestroyFence(m_device_handle, frame.in_flight_fence, nullptr);
	}
	vkDestroyCommandPool(m_device_handle, m_command_pool, nullptr);
}

Ex<void> VulkanRenderGraph::initialize(VDeviceRef device, PVSwapChain swap_chain,
									   PVImageView depth_buffer) {
	m_swap_chain = swap_chain;
	auto queue_family = device->queues().front().second;

	auto pool_flags = VCommandPoolFlag::reset_command | VCommandPoolFlag::transient;
	m_command_pool = createVkCommandPool(device, queue_family, pool_flags);
	for(auto &frame : m_frames) {
		frame.command_buffer = allocVkCommandBuffer(device, m_command_pool);
		frame.image_available_sem = createVkSemaphore(device);
		frame.render_finished_sem = createVkSemaphore(device);
		frame.in_flight_fence = createVkFence(device, true);
	}

	// TODO: handle depth buffer properly
	m_framebuffers = transform(m_swap_chain->imageViews(), [&](PVImageView image_view) {
		return VulkanFramebuffer::create(device, {image_view});
	});
	return {};
}

void VulkanRenderGraph::recreateSwapChain() {
	m_swap_chain->recreate();
	m_framebuffers = transform(m_swap_chain->imageViews(), [&](PVImageView image_view) {
		return VulkanFramebuffer::create(m_device.ref(), {image_view});
	});
}

VkFormat VulkanRenderGraph::swapChainFormat() const {
	return m_swap_chain->imageViews()[0]->format();
}

int2 VulkanRenderGraph::swapChainExtent() const { return m_swap_chain->imageViews()[0]->extent(); }

void VulkanRenderGraph::beginFrame() {
	DASSERT(m_status != Status::frame_running);
	auto &frame = m_frames[m_frame_index];

	VkFence fences[] = {frame.in_flight_fence};
	vkWaitForFences(m_device_handle, 1, fences, VK_TRUE, UINT64_MAX);

	uint image_index;
	auto result = vkAcquireNextImageKHR(m_device_handle, m_swap_chain, UINT64_MAX,
										frame.image_available_sem, VK_NULL_HANDLE, &image_index);
	if(result == VK_SUBOPTIMAL_KHR) {
		recreateSwapChain();
		result = vkAcquireNextImageKHR(m_device_handle, m_swap_chain, UINT64_MAX,
									   frame.image_available_sem, VK_NULL_HANDLE, &image_index);
	}

	// TODO: handle NOT_READY
	if(!isOneOf(result, VK_SUCCESS, VK_SUBOPTIMAL_KHR))
		FWK_VK_FATAL("vkAcquireNextImageKHR", result);
	m_image_index = image_index;

	vkResetFences(m_device_handle, 1, fences);

	vkResetCommandBuffer(frame.command_buffer, 0);
	VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	FWK_VK_CALL(vkBeginCommandBuffer, frame.command_buffer, &bi);

	m_status = Status::frame_running;
}

void VulkanRenderGraph::finishFrame() {
	DASSERT(m_status == Status::frame_running);
	flushCommands();
	m_status = Status::frame_finished;

	auto &frame = m_frames[m_frame_index];
	FWK_VK_CALL(vkEndCommandBuffer, frame.command_buffer);

	VkSemaphore waitSemaphores[] = {frame.image_available_sem};
	VkSemaphore signalSemaphores[] = {frame.render_finished_sem};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkCommandBuffer cmd_bufs[1] = {frame.command_buffer};

	VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = cmd_bufs;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	auto queues = m_device.queues();
	auto graphics_queue = queues.front().first;
	auto present_queue = queues.back().first;
	FWK_VK_CALL(vkQueueSubmit, graphics_queue, 1, &submitInfo, frame.in_flight_fence);

	VkSwapchainKHR swapChains[] = {m_swap_chain};

	VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_image_index;
	presentInfo.pResults = nullptr; // Optional

	auto present_result = vkQueuePresentKHR(present_queue, &presentInfo);
	if(present_result == VK_ERROR_OUT_OF_DATE_KHR)
		recreateSwapChain();
	else if(present_result < 0)
		FWK_VK_FATAL("vkQueuePresentKHR", present_result);
	m_frame_index = (m_frame_index + 1) % VulkanLimits::num_swap_frames;

	// TODO: staging buffers destruction should be hooked to fence
	m_staging_buffers.clear();
	m_last_pipeline_layout.reset();
	m_last_viewport = {};
}

void VulkanRenderGraph::bind(PVPipelineLayout layout) { m_last_pipeline_layout = layout; }
void VulkanRenderGraph::bindDS(int index, const VDescriptorSet &ds) {
	enqueue(CmdBindDescriptorSet{uint(index), m_last_pipeline_layout, ds.handle});
}

VDescriptorSet VulkanRenderGraph::bindDS(int index) {
	DASSERT(m_last_pipeline_layout);
	auto dsl_id = m_last_pipeline_layout->descriptorSetLayouts()[index];
	auto ds = m_device.acquireSet(dsl_id);
	enqueue(CmdBindDescriptorSet{uint(index), m_last_pipeline_layout, ds.handle});
	return ds;
}

void VulkanRenderGraph::enqueue(Command cmd) { m_commands.emplace_back(move(cmd)); }

Ex<void> VulkanRenderGraph::enqueue(CmdUpload cmd) {
	if(cmd.data.empty())
		return {};
	auto &mem_mgr = m_device.memory();
	auto mem_block = cmd.dst->memoryBlock();
	if(canBeMapped(mem_block.id.domain())) {
		mem_block.offset += cmd.offset;
		DASSERT(cmd.offset <= mem_block.size);
		// TODO: better checks
		mem_block.size = min<u32>(mem_block.size - cmd.offset, cmd.data.size());
		copy(mem_mgr.accessMemory(mem_block), cmd.data);
	} else {
		auto staging_buffer = EX_PASS(VulkanBuffer::create(
			m_device.ref(), cmd.data.size(), VBufferUsage::transfer_src, VMemoryUsage::host));
		auto mem_block = staging_buffer->memoryBlock();
		copy(mem_mgr.accessMemory(mem_block), cmd.data);
		VkBufferCopy copy_params{
			.srcOffset = 0, .dstOffset = cmd.offset, .size = (VkDeviceSize)cmd.data.size()};
		m_commands.emplace_back(
			CmdCopy{.src = staging_buffer, .dst = cmd.dst, .regions = {{copy_params}}});
		m_staging_buffers.emplace_back(move(staging_buffer));
	}

	return {};
}

Ex<void> VulkanRenderGraph::enqueue(CmdUploadImage cmd) {
	if(cmd.image.empty())
		return {};

	int data_size = cmd.image.data().size() * sizeof(IColor);

	auto &mem_mgr = m_device.memory();
	auto mem_block = cmd.dst->memoryBlock();
	if(canBeMapped(mem_block.id.domain())) {
		// TODO: better checks
		mem_block.size = min<u32>(mem_block.size, data_size);
		copy(mem_mgr.accessMemory(mem_block), cmd.image.data().reinterpret<char>());
	} else {
		auto staging_buffer = EX_PASS(VulkanBuffer::create(
			m_device.ref(), data_size, VBufferUsage::transfer_src, VMemoryUsage::host));
		auto mem_block = staging_buffer->memoryBlock();
		copy(mem_mgr.accessMemory(mem_block), cmd.image.data().reinterpret<char>());
		m_commands.emplace_back(CmdCopyImage{.src = staging_buffer, .dst = cmd.dst});
		m_staging_buffers.emplace_back(move(staging_buffer));
	}

	return {};
}

void VulkanRenderGraph::flushCommands() {
	DASSERT(m_status == Status::frame_running);
	FrameContext ctx{m_frames[m_frame_index].command_buffer, VCommandId(0)};

	for(int i : intRange(m_commands)) {
		auto &command = m_commands[i];
		ctx.cmd_id = VCommandId(i);
		command.visit([&ctx, this](auto &cmd) { perform(ctx, cmd); });
	}
	m_commands.clear();
}

VkCommandBuffer VulkanRenderGraph::currentCommandBuffer() {
	return m_frames[m_frame_index].command_buffer;
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdSetViewport &cmd) {
	VkViewport viewport{
		float(cmd.viewport.x()),	  float(cmd.viewport.y()), float(cmd.viewport.width()),
		float(cmd.viewport.height()), cmd.min_depth,		   cmd.max_depth};
	m_last_viewport = cmd.viewport;
	vkCmdSetViewport(ctx.cmd_buffer, 0, 1, &viewport);
}
void VulkanRenderGraph::perform(FrameContext &ctx, const CmdSetScissor &cmd) {
	DASSERT(cmd.scissor || !m_last_viewport.empty());
	IRect rect = cmd.scissor.orElse(m_last_viewport);
	VkRect2D vk_rect{{rect.x(), rect.y()}, {uint(rect.width()), uint(rect.height())}};
	vkCmdSetScissor(ctx.cmd_buffer, 0, 1, &vk_rect);
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBindDescriptorSet &cmd) {
	vkCmdBindDescriptorSets(ctx.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pipe_layout,
							cmd.index, 1, &cmd.set, 0, nullptr);
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBindIndexBuffer &cmd) {
	vkCmdBindIndexBuffer(ctx.cmd_buffer, cmd.buffer, cmd.offset, VkIndexType::VK_INDEX_TYPE_UINT32);
}
void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBindVertexBuffers &cmd) {
	static constexpr int max_bindings = 32;
	VkBuffer buffers[max_bindings];
	u64 offsets_64[max_bindings];

	DASSERT(cmd.buffers.size() <= max_bindings);
	for(int i : intRange(cmd.buffers)) {
		buffers[i] = cmd.buffers[i];
		offsets_64[i] = cmd.offsets[i];
	}
	vkCmdBindVertexBuffers(ctx.cmd_buffer, cmd.first_binding, cmd.buffers.size(), buffers,
						   offsets_64);
}
void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBindPipeline &cmd) {
	vkCmdBindPipeline(ctx.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pipeline);
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdDraw &cmd) {
	vkCmdDraw(ctx.cmd_buffer, cmd.num_vertices, cmd.num_instances, cmd.first_vertex,
			  cmd.first_instance);
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdDrawIndexed &cmd) {
	vkCmdDrawIndexed(ctx.cmd_buffer, cmd.num_indices, cmd.num_instances, cmd.first_index,
					 cmd.vertex_offset, cmd.first_instance);
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdCopy &cmd) {
	vkCmdCopyBuffer(ctx.cmd_buffer, cmd.src, cmd.dst, cmd.regions.size(), cmd.regions.data());
}

static void transitionImageLayout(VkCommandBuffer cmd_buffer, VkImage image, VkFormat format,
								  VImageLayout old_layout, VImageLayout new_layout) {
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = toVk(old_layout);
	barrier.newLayout = toVk(new_layout);
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
	barrier.srcAccessMask = 0; // TODO
	barrier.dstAccessMask = 0; // TODO

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

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdCopyImage &cmd) {
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	// TODO: do it properly
	auto dst_extent = cmd.dst->extent();
	region.bufferImageHeight = 0;
	region.bufferRowLength = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageExtent = {uint(dst_extent.x), uint(dst_extent.y), 1};

	auto copy_layout = VImageLayout::transfer_dst;
	auto dst_layout = cmd.dst_layout.orElse(VImageLayout::shader_ro);
	transitionImageLayout(ctx.cmd_buffer, cmd.dst, cmd.dst->format(), cmd.dst->m_last_layout,
						  copy_layout);
	vkCmdCopyBufferToImage(ctx.cmd_buffer, cmd.src, cmd.dst, toVk(copy_layout), 1, &region);

	// TODO: do this transition right before we're going to do something with this texture?
	transitionImageLayout(ctx.cmd_buffer, cmd.dst, cmd.dst->format(), copy_layout, dst_layout);
	cmd.dst->m_last_layout = dst_layout;
}

// TODO: add cache for renderpasses, add renderpass for framebuffer, interface for clear values
void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBeginRenderPass &cmd) {
	VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	bi.renderPass = cmd.render_pass;
	if(cmd.render_area)
		bi.renderArea = toVkRect(*cmd.render_area);
	else
		bi.renderArea.extent = toVkExtent(cmd.framebuffer->extent());
	bi.clearValueCount = cmd.clear_values.size();
	bi.pClearValues = cmd.clear_values.data();
	bi.framebuffer = cmd.framebuffer;
	vkCmdBeginRenderPass(ctx.cmd_buffer, &bi, VK_SUBPASS_CONTENTS_INLINE);
}
void VulkanRenderGraph::perform(FrameContext &ctx, const CmdEndRenderPass &cmd) {
	vkCmdEndRenderPass(ctx.cmd_buffer);
}
}