// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/format.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/font_factory.h>
#include <fwk/gfx/font_finder.h>
#include <fwk/gfx/gl_texture.h>
#include <fwk/gfx/renderer2d.h>
#include <fwk/gfx/shader_compiler.h>
#include <fwk/io/file_system.h>
#include <fwk/sys/expected.h>
#include <fwk/sys/input.h>
#include <fwk/vulkan/vulkan_buffer.h>
#include <fwk/vulkan/vulkan_device.h>
#include <fwk/vulkan/vulkan_framebuffer.h>
#include <fwk/vulkan/vulkan_image.h>
#include <fwk/vulkan/vulkan_instance.h>
#include <fwk/vulkan/vulkan_pipeline.h>
#include <fwk/vulkan/vulkan_swap_chain.h>
#include <fwk/vulkan/vulkan_window.h>

#include <vulkan/vulkan.h>

// TODO: handling VkResults
// TODO: making sure that handles are correct ?
// TODO: more type safety
// TODO: VHandle<> class ? handles are not initialized to null by default ...
// TODO: kolejno�� niszczenia obiekt�w...

using namespace fwk;

const char *vertex_shader = R"(
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
	gl_Position = vec4(inPosition, 0.0, 1.0);
	fragColor = inColor;
}
)";

const char *fragment_shader = R"(
#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;

void main() {
	outColor = vec4(fragColor, 1.0);
}
)";

string fontPath() {
	if(platform == Platform::html)
		return "data/LiberationSans-Regular.ttf";
	return findDefaultSystemFont().get().file_path;
}

struct MyVertex {
	float2 pos;
	float3 color;

	static void addStreamDesc(vector<VertexBindingDesc> &bindings,
							  vector<VertexAttribDesc> &attribs, int index, int first_location) {
		bindings.emplace_back(VertexBindingDesc{.index = u8(index), .stride = sizeof(MyVertex)});
		attribs.emplace_back(VertexAttribDesc{
			.format = VkFormat::VK_FORMAT_R32G32_SFLOAT,
			.offset = 0,
			.location_index = u8(first_location),
			.binding_index = u8(index),
		});
		attribs.emplace_back(VertexAttribDesc{
			.format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
			.offset = sizeof(pos),
			.location_index = u8(first_location + 1),
			.binding_index = u8(index),
		});
	}
};

struct VulkanContext {
	VDeviceRef device;
	VWindowRef window;
	PVSwapChain swap_chain;
	PVPipeline pipeline;
	vector<PVFramebuffer> framebuffers;
	PVCommandPool command_pool;
	PVCommandBuffer command_buffer;
	PVBuffer vertex_buffer;
	uint num_vertices;

	PVSemaphore imageAvailableSemaphore;
	PVSemaphore renderFinishedSemaphore;
	PVFence inFlightFence;

	Maybe<Font> font;
};

Ex<void> createVertexBuffer(VulkanContext &ctx) {
	vector<MyVertex> vertices = {{{0.0f, -0.75f}, {1.0f, 0.0f, 0.0f}},
								 {{0.75f, 0.75f}, {0.0f, 1.0f, 0.0f}},
								 {{-0.75f, 0.75f}, {0.0f, 0.0f, 1.0f}}};

	auto usage = VBufferUsageFlag::vertex_buffer;
	auto buffer = EX_PASS(VulkanBuffer::create<MyVertex>(ctx.device, vertices.size(), usage));
	auto mem_req = buffer->memoryRequirements();
	auto mem_flags = VMemoryFlag::host_visible | VMemoryFlag::host_coherent;
	auto memory =
		EX_PASS(ctx.device->allocDeviceMemory(mem_req.size, mem_req.memoryTypeBits, mem_flags));
	EXPECT(buffer->bindMemory(memory));
	buffer->upload(vertices);
	ctx.vertex_buffer = move(buffer); // TODO: just copy doesnt work
	ctx.num_vertices = vertices.size();
	return {};
}

Ex<PVPipeline> createPipeline(VDeviceRef device, PVSwapChain swap_chain) {
	// TODO: making sure that shaderc_shared.dll is available
	ShaderCompiler compiler;
	auto vsh_code = compiler.compile(ShaderType::vertex, vertex_shader);
	auto fsh_code = compiler.compile(ShaderType::fragment, fragment_shader);

	if(!vsh_code.bytecode)
		return ERROR("Failed to compile vertex shader:\n%", vsh_code.messages);
	if(!fsh_code.bytecode)
		return ERROR("Failed to compile fragment shader:\n%", fsh_code.messages);

	auto vsh_module = EX_PASS(device->createShaderModule(vsh_code.bytecode));
	auto fsh_module = EX_PASS(device->createShaderModule(fsh_code.bytecode));

	auto sc_image = swap_chain->imageViews().front()->image();
	auto extent = sc_image->extent();
	VPipelineSetup setup;
	setup.stages.emplace_back(vsh_module, VShaderStage::vertex);
	setup.stages.emplace_back(fsh_module, VShaderStage::fragment);
	setup.viewport = {.x = 0.0f,
					  .y = 0.0f,
					  .width = (float)extent.width,
					  .height = (float)extent.height,
					  .minDepth = 0.0f,
					  .maxDepth = 1.0f};
	setup.scissor = {.offset = {0, 0}, .extent = extent};

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = sc_image->format();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	setup.render_pass = EX_PASS(VulkanRenderPass::create(
		device, cspan(&colorAttachment, 1), cspan(&subpass, 1), cspan(&dependency, 1)));

	MyVertex::addStreamDesc(setup.vertex_bindings, setup.vertex_attribs, 0, 0);

	return VulkanPipeline::create(device, setup);
}

Ex<void> createCommandBuffers(VulkanContext &ctx) {
	auto queue_family = ctx.device->queues().front().second;
	ctx.command_pool =
		EX_PASS(ctx.device->createCommandPool(queue_family, CommandPoolFlag::reset_command));
	ctx.command_buffer = EX_PASS(ctx.command_pool->allocBuffer());
	return {};
}

Ex<void> recordCommandBuffer(VulkanContext &ctx, uint32_t imageIndex) {
	vkResetCommandBuffer(ctx.command_buffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if(vkBeginCommandBuffer(ctx.command_buffer, &beginInfo) != VK_SUCCESS) {
		return ERROR("failed begin");
	}

	VkRenderPassBeginInfo renderPassInfo{};
	auto extent = ctx.framebuffers[imageIndex]->extent();
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = ctx.pipeline->renderPass();
	renderPassInfo.framebuffer = ctx.framebuffers[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = extent;

	VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(ctx.command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(ctx.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline);

	VkBuffer vertexBuffers[] = {ctx.vertex_buffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(ctx.command_buffer, 0, 1, vertexBuffers, offsets);
	vkCmdDraw(ctx.command_buffer, static_cast<uint32_t>(ctx.num_vertices), 1, 0, 0);

	vkCmdEndRenderPass(ctx.command_buffer);
	if(vkEndCommandBuffer(ctx.command_buffer) != VK_SUCCESS)
		return ERROR("endCOmand failed");

	return {};
}

Ex<void> createSyncObjects(VulkanContext &ctx) {
	ctx.imageAvailableSemaphore = EX_PASS(ctx.device->createSemaphore());
	ctx.renderFinishedSemaphore = EX_PASS(ctx.device->createSemaphore());
	ctx.inFlightFence = EX_PASS(ctx.device->createFence(true));

	return {};
}

bool mainLoop(VulkanWindow &window, void *ctx_) {
	auto &ctx = *(VulkanContext *)ctx_;
	static vector<float2> positions(15, float2(window.extent() / 2));

	for(auto &event : window.inputEvents()) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEventType::quit)
			return false;

		if(event.isMouseOverEvent() && (event.mouseMove() != int2(0, 0)))
			positions.emplace_back(float2(event.mousePos()));
	}

	while(positions.size() > 15)
		positions.erase(positions.begin());

	auto swap_chain = ctx.swap_chain;
	VkFence fences[] = {ctx.inFlightFence};
	vkWaitForFences(ctx.device->handle(), 1, fences, VK_TRUE, UINT64_MAX);
	vkResetFences(ctx.device->handle(), 1, fences);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(ctx.device->handle(), swap_chain, UINT64_MAX, ctx.imageAvailableSemaphore,
						  VK_NULL_HANDLE, &imageIndex);
	recordCommandBuffer(ctx, imageIndex).check();

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {ctx.imageAvailableSemaphore};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	VkCommandBuffer cmd_bufs[1] = {ctx.command_buffer};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = cmd_bufs;

	VkSemaphore signalSemaphores[] = {ctx.renderFinishedSemaphore};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	auto queues = ctx.device->queues();
	auto graphicsQueue = queues.front().first;
	auto presentQueue = queues.back().first;
	if(vkQueueSubmit(graphicsQueue, 1, &submitInfo, ctx.inFlightFence) != VK_SUCCESS)
		FWK_FATAL("queue submit fail");

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = {swap_chain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional
	vkQueuePresentKHR(presentQueue, &presentInfo);

	//clearColor(IColor(50, 0, 50));
	/*Renderer2D renderer(IRect(VkDevice::instance().windowSize()), Orient2D::y_down);

	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect({-50, -50}, {50, 50}) + positions[n];
		FColor fill_color(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f);
		IColor border_color = ColorId::black;

		renderer.addFilledRect(rect, fill_color);
		renderer.addRect(rect, border_color);
	}

	auto text = format("Hello world!\nWindow size: %", device.windowSize());
	font.draw(renderer, FRect({5, 5}, {200, 20}), {ColorId::white}, text);
	renderer.render();*/

	// TODO: is this a good place ?
	ctx.device->nextReleasePhase();

	return true;
}

Ex<int> exMain() {
	VulkanInstanceSetup setup;
	setup.debug_levels = VDebugLevel::warning | VDebugLevel::error;
	setup.debug_types = all<VDebugType>;
	auto instance = EX_PASS(VulkanInstance::create(setup));

	auto flags = VWindowFlag::resizable | VWindowFlag::vsync | VWindowFlag::centered |
				 VWindowFlag::allow_hidpi;
	auto window = EX_PASS(VulkanWindow::create(instance, "fwk::vulkan_test", IRect(0, 0, 1280, 720),
											   VulkanWindowConfig{flags}));

	VulkanDeviceSetup dev_setup;
	auto pref_device = instance->preferredDevice(window->surfaceHandle(), &dev_setup.queues);
	if(!pref_device)
		return ERROR("Couldn't find a suitable Vulkan device");
	auto device = EX_PASS(instance->createDevice(*pref_device, dev_setup));

	auto surf_info = VulkanSwapChain::surfaceInfo(device, window);
	auto swap_chain =
		EX_PASS(VulkanSwapChain::create(device, window,
										{.surface_format = surf_info.formats[0],
										 .present_mode = surf_info.present_modes[0],
										 .transform = surf_info.capabilities.currentTransform}));

	VulkanContext ctx{device, window, swap_chain};
	ctx.pipeline = EX_PASS(createPipeline(device, swap_chain));
	for(auto &image_view : swap_chain->imageViews())
		ctx.framebuffers.emplace_back(EX_PASS(
			VulkanFramebuffer::create(device, cspan(&image_view, 1), ctx.pipeline->renderPass())));

	EXPECT(createCommandBuffers(ctx));
	EXPECT(createSyncObjects(ctx));
	EXPECT(createVertexBuffer(ctx));

	int font_size = 16 * window->dpiScale();
	//ctx.font = FontFactory().makeFont(fontPath(), font_size);
	window->runMainLoop(mainLoop, &ctx);
	return 0;
}

int main(int argc, char **argv) {
	auto result = exMain();

	if(!result) {
		result.error().print();
		return 1;
	}
	return *result;
}
