// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_pipeline.h"

#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_storage.h"

#include <vulkan/vulkan.h>

namespace fwk {

static const EnumMap<VShaderStage, VkShaderStageFlagBits> shader_stages = {{
	{VShaderStage::vertex, VK_SHADER_STAGE_VERTEX_BIT},
	{VShaderStage::fragment, VK_SHADER_STAGE_FRAGMENT_BIT},
	{VShaderStage::compute, VK_SHADER_STAGE_COMPUTE_BIT},
}};

static const EnumMap<VTopology, VkPrimitiveTopology> primitive_topos = {{
	{VTopology::point_list, VK_PRIMITIVE_TOPOLOGY_POINT_LIST},
	{VTopology::line_list, VK_PRIMITIVE_TOPOLOGY_LINE_LIST},
	{VTopology::line_strip, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP},
	{VTopology::triangle_list, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
	{VTopology::triangle_strip, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP},
	{VTopology::triangle_fan, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN},
}};

Ex<PVRenderPass> VulkanRenderPass::create(VDeviceRef device,
										  CSpan<VkAttachmentDescription> attachments,
										  CSpan<VkSubpassDescription> subpasses,
										  CSpan<VkSubpassDependency> dependencies) {
	VkRenderPassCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	ci.attachmentCount = attachments.size();
	ci.pAttachments = attachments.data();
	ci.subpassCount = subpasses.size();
	ci.pSubpasses = subpasses.data();
	ci.dependencyCount = dependencies.size();
	ci.pDependencies = dependencies.data();
	VkRenderPass handle;
	if(vkCreateRenderPass(device->handle(), &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateRenderPass failed");
	auto out = g_vk_storage.allocObject(device, handle);
	out->m_attachment_count = attachments.size();
	out->m_subpass_count = subpasses.size();
	out->m_dependency_count = dependencies.size();
	return out;
}

VulkanPipeline::VulkanPipeline(PVRenderPass rp, PVPipelineLayout lt)
	: m_render_pass(rp), m_pipeline_layout(lt) {}
VulkanPipeline::~VulkanPipeline() = default;

Ex<PVPipelineLayout> VulkanPipeline::createLayout(VDeviceRef device) {
	VkPipelineLayoutCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	// TODO: expose
	ci.setLayoutCount = 0;
	ci.pSetLayouts = nullptr;
	ci.pushConstantRangeCount = 0;
	ci.pPushConstantRanges = nullptr;

	VkPipelineLayout handle;
	if(vkCreatePipelineLayout(device->handle(), &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreatePipelineLayout failed");
	return g_vk_storage.allocObject(device, handle);
}

Ex<PVPipeline> VulkanPipeline::create(VDeviceRef device, const VPipelineSetup &setup) {
	DASSERT(setup.render_pass);
	auto pipeline_layout = EX_PASS(createLayout(device));

	array<VkPipelineShaderStageCreateInfo, count<VShaderStage>> stages_ci;
	for(int i : intRange(setup.stages)) {
		DASSERT(setup.stages[i].shader_module);
		stages_ci[i] = {};
		stages_ci[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages_ci[i].stage = shader_stages[setup.stages[i].stage];
		stages_ci[i].module = setup.stages[i].shader_module;
		stages_ci[i].pName = "main";
	}

	VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
	vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_ci.vertexBindingDescriptionCount = 0;
	vertex_input_ci.pVertexBindingDescriptions = nullptr; // Optional
	vertex_input_ci.vertexAttributeDescriptionCount = 0;
	vertex_input_ci.pVertexAttributeDescriptions = nullptr; // Optional

	VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
	input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_ci.topology = primitive_topos[setup.input_assembly.topology];
	input_assembly_ci.primitiveRestartEnable =
		setup.input_assembly.primitive_restart ? VK_TRUE : VK_FALSE;

	VkPipelineViewportStateCreateInfo viewport_state_ci{};
	viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_ci.viewportCount = 1;
	viewport_state_ci.pViewports = &setup.viewport;
	viewport_state_ci.scissorCount = 1;
	viewport_state_ci.pScissors = &setup.scissor;

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

	VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(arraySize(dynamicStates));
	dynamicState.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	ci.stageCount = setup.stages.size();
	ci.pStages = stages_ci.data();
	ci.pVertexInputState = &vertex_input_ci;
	ci.pInputAssemblyState = &input_assembly_ci;
	ci.pViewportState = &viewport_state_ci;
	ci.pRasterizationState = &rasterizer_ci;
	ci.pMultisampleState = &multisampling;
	ci.pDepthStencilState = nullptr; // Optional
	ci.pColorBlendState = &colorBlending;
	ci.pDynamicState = nullptr; // Optional
	ci.layout = pipeline_layout;
	ci.renderPass = setup.render_pass;
	ci.subpass = 0;
	ci.basePipelineHandle = VK_NULL_HANDLE; // Optional
	ci.basePipelineIndex = -1; // Optional

	VkPipeline handle;
	if(vkCreateGraphicsPipelines(device->handle(), nullptr, 1, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateGraphicsPipelines failed");
	return g_vk_storage.allocObject(device, handle, setup.render_pass, pipeline_layout);
}
}