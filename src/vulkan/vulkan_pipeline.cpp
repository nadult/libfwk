// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_pipeline.h"

#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_device.h"

#include <vulkan/vulkan.h>

namespace fwk {

VulkanRenderPass::VulkanRenderPass(VkRenderPass handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}
VulkanRenderPass ::~VulkanRenderPass() {
	// TODO: here we probably don't need defer
	deferredHandleRelease<VkRenderPass, vkDestroyRenderPass>();
}

VulkanPipelineLayout::VulkanPipelineLayout(VkPipelineLayout handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}
VulkanPipelineLayout ::~VulkanPipelineLayout() {
	// TODO: do we really need deferred?
	deferredHandleRelease<VkPipelineLayout, vkDestroyPipelineLayout>();
}

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VkDescriptorSetLayout handle, VObjectId id,
													 vector<BindingInfo> bindings)
	: VulkanObjectBase(handle, id), m_bindings(move(bindings)) {}
VulkanDescriptorSetLayout ::~VulkanDescriptorSetLayout() {
	// TODO: do we really need deferred?
	deferredHandleRelease<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>();
}

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
	auto out = device->createObject(handle);
	out->m_attachment_count = attachments.size();
	out->m_subpass_count = subpasses.size();
	out->m_dependency_count = dependencies.size();
	return out;
}

VulkanPipeline::VulkanPipeline(VkPipeline handle, VObjectId id, PVRenderPass rp,
							   PVPipelineLayout lt)
	: VulkanObjectBase(handle, id), m_render_pass(rp), m_pipeline_layout(lt) {}
VulkanPipeline::~VulkanPipeline() { deferredHandleRelease<VkPipeline, vkDestroyPipeline>(); }

Ex<PVDescriptorSetLayout>
VulkanPipeline::createDescriptorSetLayout(VDeviceRef device,
										  vector<DescriptorBindingInfo> bindings) {
	auto vk_bindings = transform(bindings, [](const DescriptorBindingInfo &binding) {
		VkDescriptorSetLayoutBinding lb{};
		lb.binding = binding.index;
		lb.descriptorType = toVk(binding.type);
		lb.descriptorCount = binding.count;
		lb.stageFlags = toVk(binding.stages);
		return lb;
	});

	VkDescriptorSetLayoutCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ci.pBindings = vk_bindings.data();
	ci.bindingCount = vk_bindings.size();

	VkDescriptorSetLayout handle;
	if(vkCreateDescriptorSetLayout(device, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateDescriptorSetLayout failed");
	return device->createObject(handle, move(bindings));
}

Ex<PVPipelineLayout> VulkanPipeline::createLayout(VDeviceRef device,
												  CSpan<PVDescriptorSetLayout> set_layouts) {
	VkPipelineLayoutCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	auto sl_handles = transform<VkDescriptorSetLayout>(set_layouts);
	ci.setLayoutCount = set_layouts.size();
	ci.pSetLayouts = sl_handles.data();
	ci.pushConstantRangeCount = 0;
	ci.pPushConstantRanges = nullptr;

	VkPipelineLayout handle;
	if(vkCreatePipelineLayout(device->handle(), &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreatePipelineLayout failed");
	return device->createObject(handle);
}

Ex<PVPipeline> VulkanPipeline::create(VDeviceRef device, const VPipelineSetup &setup) {
	DASSERT(setup.render_pass);
	auto pipeline_layout = EX_PASS(createLayout(device, setup.dsls));

	array<VkPipelineShaderStageCreateInfo, count<VShaderStage>> stages_ci;
	for(int i : intRange(setup.stages)) {
		DASSERT(setup.stages[i].shader_module);
		stages_ci[i] = {};
		stages_ci[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages_ci[i].stage = toVk(setup.stages[i].stage);
		stages_ci[i].module = setup.stages[i].shader_module;
		stages_ci[i].pName = "main";
	}

	auto vertex_bindings = transform(setup.vertex_bindings, [](const VertexBindingDesc &desc) {
		VkVertexInputBindingDescription out;
		out.binding = desc.index;
		out.inputRate = desc.input_rate == VertexInputRate::vertex ? VK_VERTEX_INPUT_RATE_VERTEX :
																	   VK_VERTEX_INPUT_RATE_INSTANCE;
		out.stride = desc.stride;
		return out;
	});

	auto vertex_attribs = transform(setup.vertex_attribs, [](const VertexAttribDesc &desc) {
		VkVertexInputAttributeDescription out;
		out.binding = desc.binding_index;
		out.format = desc.format;
		out.location = desc.location_index;
		out.offset = desc.offset;
		return out;
	});

	VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
	vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_ci.vertexBindingDescriptionCount = vertex_bindings.size();
	vertex_input_ci.pVertexBindingDescriptions = vertex_bindings.data();
	vertex_input_ci.vertexAttributeDescriptionCount = vertex_attribs.size();
	vertex_input_ci.pVertexAttributeDescriptions = vertex_attribs.data();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
	input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_ci.topology = toVk(setup.input_assembly.topology);
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
	return device->createObject(handle, setup.render_pass, pipeline_layout);
}
}