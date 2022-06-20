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
#include <fwk/vulkan/vulkan_instance.h>
#include <fwk/vulkan/vulkan_window.h>

#include <vulkan/vulkan.h>

// TODO: handling VkResults
// TODO: making sure that handles are correct ?
// TODO: more type safety
// TODO: VHandle<> class ? handles are not initialized to null by default ...
// TODO: kolejnoœæ niszczenia obiektów...

using namespace fwk;

const char *vertex_shader = R"(
#version 450
vec2 positions[3] = vec2[](
	vec2(0.0, -0.5),
	vec2(0.5, 0.5),
	vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 fragColor;

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	fragColor = colors[gl_VertexIndex];
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

Ex<VkShaderModule> createShaderModule(VkDevice device_handle, CSpan<char> bytecode) {
	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = bytecode.size();
	// TODO: make sure that this is safe
	ci.pCode = reinterpret_cast<const uint32_t *>(bytecode.data());

	VkShaderModule handle;
	if(vkCreateShaderModule(device_handle, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateShaderModule failed");
	return handle;
}

struct Pipeline {
	VkPipeline graphicsPipeline;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
};

Ex<Pipeline> createPipeline(VDeviceId device_id, const VulkanSwapChainInfo &swap_chain) {
	Pipeline out;

	auto &instance = VulkanInstance::instance();
	auto device_handle = instance[device_id].handle;

	// TODO: making sure that shaderc_shared.dll is available
	ShaderCompiler compiler;
	auto vsh_code = compiler.compile(ShaderType::vertex, vertex_shader);
	auto fsh_code = compiler.compile(ShaderType::fragment, fragment_shader);

	if(!vsh_code.bytecode)
		return ERROR("Failed to compile vertex shader:\n%", vsh_code.messages);
	if(!fsh_code.bytecode)
		return ERROR("Failed to compile fragment shader:\n%", fsh_code.messages);

	auto vsh_module = EX_PASS(createShaderModule(device_handle, vsh_code.bytecode));
	auto fsh_module = EX_PASS(createShaderModule(device_handle, fsh_code.bytecode));

	VkPipelineShaderStageCreateInfo vsh_stage_ci{};
	vsh_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vsh_stage_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vsh_stage_ci.module = vsh_module;
	vsh_stage_ci.pName = "main";

	VkPipelineShaderStageCreateInfo fsh_stage_ci{};
	fsh_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fsh_stage_ci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fsh_stage_ci.module = fsh_module;
	fsh_stage_ci.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages_ci[] = {vsh_stage_ci, fsh_stage_ci};

	VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
	vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_ci.vertexBindingDescriptionCount = 0;
	vertex_input_ci.pVertexBindingDescriptions = nullptr; // Optional
	vertex_input_ci.vertexAttributeDescriptionCount = 0;
	vertex_input_ci.pVertexAttributeDescriptions = nullptr; // Optional

	VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
	input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_ci.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swap_chain.extent.width;
	viewport.height = (float)swap_chain.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = swap_chain.extent;

	VkPipelineViewportStateCreateInfo viewport_state_ci{};
	viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_ci.viewportCount = 1;
	viewport_state_ci.pViewports = &viewport;
	viewport_state_ci.scissorCount = 1;
	viewport_state_ci.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer_ci{};
	rasterizer_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_ci.depthClampEnable = VK_FALSE;
	rasterizer_ci.rasterizerDiscardEnable = VK_FALSE;
	rasterizer_ci.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_ci.lineWidth = 1.0f;
	rasterizer_ci.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer_ci.depthBiasEnable = VK_FALSE;
	rasterizer_ci.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer_ci.depthBiasClamp = 0.0f; // Optional
	rasterizer_ci.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineColorBlendAttachmentState color_blend_attachment{};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
											VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &color_blend_attachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0; // Optional
	pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

	if(vkCreatePipelineLayout(device_handle, &pipelineLayoutInfo, nullptr, &out.pipelineLayout) !=
	   VK_SUCCESS)
		return ERROR("vkCreatePipelineLayout failed");

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swap_chain.format;
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

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if(vkCreateRenderPass(device_handle, &renderPassInfo, nullptr, &out.renderPass) != VK_SUCCESS) {
		return ERROR("vkCreateRenderPass failed");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shader_stages_ci;
	pipelineInfo.pVertexInputState = &vertex_input_ci;
	pipelineInfo.pInputAssemblyState = &input_assembly_ci;
	pipelineInfo.pViewportState = &viewport_state_ci;
	pipelineInfo.pRasterizationState = &rasterizer_ci;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = out.pipelineLayout;
	pipelineInfo.renderPass = out.renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	if(vkCreateGraphicsPipelines(device_handle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
								 &out.graphicsPipeline) != VK_SUCCESS) {
		return ERROR("vkCreateGraphicsPipelines failed");
	}

	// TODO: proper destruction
	vkDestroyShaderModule(device_handle, vsh_module, nullptr);
	vkDestroyShaderModule(device_handle, fsh_module, nullptr);

	return out;
}

void destroyGraphicsPipeline(VkDevice device_handle, Pipeline &pipeline) {
	vkDestroyPipeline(device_handle, pipeline.graphicsPipeline, nullptr);
	vkDestroyRenderPass(device_handle, pipeline.renderPass, nullptr);
	vkDestroyPipelineLayout(device_handle, pipeline.pipelineLayout, nullptr);
}

struct Framebuffers {
	vector<VkFramebuffer> swapChainFramebuffers;
};

Ex<Framebuffers> createFramebuffers(VkDevice handle_device, const VulkanSwapChainInfo &swap_chain,
									const Pipeline &pipeline) {
	Framebuffers out;
	out.swapChainFramebuffers.resize(swap_chain.images.size());

	for(size_t i = 0; i < swap_chain.image_views.size(); i++) {
		VkImageView attachments[] = {swap_chain.image_views[i]};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = pipeline.renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swap_chain.extent.width;
		framebufferInfo.height = swap_chain.extent.height;
		framebufferInfo.layers = 1;

		if(vkCreateFramebuffer(handle_device, &framebufferInfo, nullptr,
							   &out.swapChainFramebuffers[i]) != VK_SUCCESS) {
			return ERROR("vkCreateFramebuffer failed");
		}
	}

	return out;
}

void destroyFramebuffers(VkDevice device_handle, Framebuffers &fbs) {
	for(auto fb : fbs.swapChainFramebuffers)
		vkDestroyFramebuffer(device_handle, fb, nullptr);
	fbs.swapChainFramebuffers.clear();
}

struct CommandBuffers {
	VkCommandPool pool;
	VkCommandBuffer buffer;
};

Ex<CommandBuffers> createCommandBuffers(VDeviceId device_id) {
	CommandBuffers out;

	auto &vulkan = VulkanInstance::instance();
	auto &device_info = vulkan[device_id];

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = device_info.queues[0].second;
	if(vkCreateCommandPool(device_info.handle, &poolInfo, nullptr, &out.pool) != VK_SUCCESS) {
		return ERROR("vkCreateCommandPool failed");
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = out.pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if(vkAllocateCommandBuffers(device_info.handle, &allocInfo, &out.buffer) != VK_SUCCESS) {
		return ERROR("alloc failed");
	}

	return out;
}

Ex<void> recordCommandBuffer(VkCommandBuffer commandBuffer, Pipeline &pipeline, Framebuffers &fbs,
							 const VulkanSwapChainInfo &swap_chain, uint32_t imageIndex) {
	vkResetCommandBuffer(commandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		return ERROR("failed begin");
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = pipeline.renderPass;
	renderPassInfo.framebuffer = fbs.swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = swap_chain.extent;

	VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(commandBuffer);

	if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		return ERROR("endCOmand failed");
	}

	return {};
}

void destroyCommandBuffers(VkDevice device_handle, CommandBuffers &bufs) {
	vkDestroyCommandPool(device_handle, bufs.pool, nullptr);
}

struct VulkanContext {
	VDeviceId device_id;
	VulkanWindow *window;
	Pipeline pipeline;
	Framebuffers framebuffers;
	CommandBuffers commands;

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;

	Maybe<Font> font;
};

Ex<void> createSyncObjects(VulkanContext &ctx) {
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	auto device = VulkanInstance::instance()[ctx.device_id].handle;
	if(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &ctx.imageAvailableSemaphore) !=
		   VK_SUCCESS ||
	   vkCreateSemaphore(device, &semaphoreInfo, nullptr, &ctx.renderFinishedSemaphore) !=
		   VK_SUCCESS ||
	   vkCreateFence(device, &fenceInfo, nullptr, &ctx.inFlightFence) != VK_SUCCESS) {
		return ERROR("Failed to create syncs");
	}

	// TODO: destroy them

	return {};
}

void destroySyncObjects(VkDevice device_handle, VulkanContext &ctx) {
	vkDestroySemaphore(device_handle, ctx.imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(device_handle, ctx.renderFinishedSemaphore, nullptr);
	vkDestroyFence(device_handle, ctx.inFlightFence, nullptr);
}

bool mainLoop(VulkanWindow &window, void *ctx_) {
	auto &ctx = *(VulkanContext *)ctx_;
	static vector<float2> positions(15, float2(window.size() / 2));

	for(auto &event : window.inputEvents()) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEventType::quit)
			return false;

		if(event.isMouseOverEvent() && (event.mouseMove() != int2(0, 0)))
			positions.emplace_back(float2(event.mousePos()));
	}

	while(positions.size() > 15)
		positions.erase(positions.begin());

	auto &vulkan = VulkanInstance::instance();
	auto device = vulkan[ctx.device_id].handle;
	auto &swap_chain = ctx.window->swapChain();

	vkWaitForFences(device, 1, &ctx.inFlightFence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &ctx.inFlightFence);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(device, swap_chain.handle, UINT64_MAX, ctx.imageAvailableSemaphore,
						  VK_NULL_HANDLE, &imageIndex);
	recordCommandBuffer(ctx.commands.buffer, ctx.pipeline, ctx.framebuffers, window.swapChain(),
						imageIndex)
		.check();

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {ctx.imageAvailableSemaphore};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &ctx.commands.buffer;

	VkSemaphore signalSemaphores[] = {ctx.renderFinishedSemaphore};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	auto graphicsQueue = vulkan[ctx.device_id].queues.front().first;
	auto presentQueue = vulkan[ctx.device_id].queues.back().first;
	if(vkQueueSubmit(graphicsQueue, 1, &submitInfo, ctx.inFlightFence) != VK_SUCCESS)
		FWK_FATAL("queue submit fail");

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = {swap_chain.handle};
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

	return true;
}

Ex<int> exMain() {
	double time = getTime();

	VulkanInstance instance;
	{
		VulkanInstanceSetup setup;
		setup.debug_levels = VDebugLevel::warning | VDebugLevel::error;
		setup.debug_types = all<VDebugType>;
		EXPECT(instance.initialize(setup));
	}

	auto flags = VWindowFlag::resizable | VWindowFlag::vsync | VWindowFlag::centered |
				 VWindowFlag::allow_hidpi;
	auto window = EX_PASS(construct<VulkanWindow>("fwk::vulkan_test", IRect(0, 0, 1280, 720),
												  VulkanWindowConfig{flags}));

	VulkanDeviceSetup setup;
	auto pref_device = instance.preferredDevice(window.surfaceHandle(), &setup.queues);
	if(!pref_device)
		return ERROR("Coudln't find a suitable Vulkan device");
	auto device_id = EX_PASS(instance.createDevice(*pref_device, setup));
	auto device_handle = instance[device_id].handle;
	EXPECT(window.createSwapChain(device_id));

	VulkanContext ctx{device_id, &window};
	ctx.pipeline = EX_PASS(createPipeline(device_id, window.swapChain()));
	ctx.framebuffers = EX_PASS(createFramebuffers(device_handle, window.swapChain(), ctx.pipeline));
	ctx.commands = EX_PASS(createCommandBuffers(device_id));

	EXPECT(createSyncObjects(ctx));

	int font_size = 16 * window.dpiScale();
	//ctx.font = FontFactory().makeFont(fontPath(), font_size);
	window.runMainLoop(mainLoop, &ctx);

	vkDeviceWaitIdle(device_handle);

	destroySyncObjects(device_handle, ctx);
	destroyCommandBuffers(device_handle, ctx.commands);
	destroyFramebuffers(device_handle, ctx.framebuffers);
	destroyGraphicsPipeline(device_handle, ctx.pipeline);

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
