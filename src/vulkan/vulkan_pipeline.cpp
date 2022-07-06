// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_pipeline.h"

#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_shader.h"

#include <vulkan/vulkan.h>

namespace fwk {

vector<DescriptorBindingInfo> DescriptorBindingInfo::merge(CSpan<DescriptorBindingInfo> lhs,
														   CSpan<DescriptorBindingInfo> rhs) {
	vector<DescriptorBindingInfo> out;
	out.reserve(lhs.size() + rhs.size());

	int lpos = 0, rpos = 0;
	while(lpos < lhs.size() && rpos < rhs.size()) {
		auto &left = lhs[lpos];
		auto &right = rhs[lpos];

		if((left.value & ~stages_bit_mask) == (right.value & ~stages_bit_mask)) {
			out.emplace_back(left.value | (right.value & stages_bit_mask));
			lpos++, rpos++;
			continue;
		}
		if(left < right) {
			out.emplace_back(left);
			lpos++;
		} else {
			out.emplace_back(right);
			rpos++;
		}
	}

	if(lpos < lhs.size())
		insertBack(out, cspan(lhs.data() + lpos, lhs.size() - lpos));
	if(rpos < rhs.size())
		insertBack(out, cspan(rhs.data() + rpos, rhs.size() - rpos));
	return out;
}

u32 DescriptorBindingInfo::hashIgnoreSet(CSpan<DescriptorBindingInfo> infos, u32 seed) {
	u64 hash = seed;
	for(auto info : infos) {
		info.clearSet();
		hash = combineHash(hash, hashU64(info.value));
	}

	// TODO: hash 32 -> 64
	return u32(hash);
}

// TODO: what about set indices? we have to pass them forward too somehow
vector<CSpan<DescriptorBindingInfo>>
DescriptorBindingInfo::divideSets(CSpan<DescriptorBindingInfo> merged) {
	vector<CSpan<DescriptorBindingInfo>> out;
	uint prev_set = merged.empty() ? 0 : merged[0].set();
	int start_idx = 0;

	for(int i : intRange(merged)) {
		auto cur_set = merged[i].set();
		if(cur_set != prev_set) {
			out.emplace_back(merged.data() + start_idx, merged.data() + i);
			start_idx = i;
			prev_set = cur_set;
		}
	}

	out.emplace_back(merged.data() + start_idx, merged.end());
	return out;
}

VulkanSampler::VulkanSampler(VkSampler handle, VObjectId id, const VSamplingParams &params)
	: VulkanObjectBase(handle, id), m_params(params) {}
VulkanSampler::~VulkanSampler() { deferredHandleRelease<VkSampler, vkDestroySampler>(); }

void DescriptorSet::update(CSpan<Assignment> assigns) {
	DASSERT(assigns.size() <= max_assignments);

	array<VkDescriptorBufferInfo, max_assignments> buffer_infos;
	array<VkDescriptorImageInfo, max_assignments> image_infos;
	array<VkWriteDescriptorSet, max_assignments> writes;
	int num_buffers = 0, num_images = 0;

	for(int i : intRange(assigns)) {
		auto &write = writes[i];
		auto &assign = assigns[i];
		write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstBinding = assign.binding;
		write.dstSet = handle;
		write.descriptorType = toVk(assign.type);
		write.descriptorCount = 1;
		if(const PVBuffer *pbuffer = assign.data) {
			auto &bi = buffer_infos[num_buffers++];
			bi = {};
			bi.buffer = *pbuffer;
			bi.offset = 0;
			bi.range = VK_WHOLE_SIZE;
			write.pBufferInfo = &bi;
		} else if(const Pair<PVSampler, PVImageView> *pair = assign.data) {
			auto &ii = image_infos[num_images++];
			ii = {};
			ii.imageView = pair->second;
			ii.sampler = pair->first;
			ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			write.pImageInfo = &ii;
		}
	}
	vkUpdateDescriptorSets(pool->deviceHandle(), assigns.size(), writes.data(), 0, nullptr);
}

Ex<DescriptorSet> VulkanDescriptorPool::alloc(PVPipelineLayout layout, uint index) {
	VkDescriptorSetLayout layout_handle = layout->descriptorSetLayouts()[index];
	VkDescriptorSetAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = m_handle;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &layout_handle;
	VkDescriptorSet handle;
	auto result = vkAllocateDescriptorSets(deviceHandle(), &ai, &handle);
	if(result != VK_SUCCESS)
		return ERROR("vkAllocateDescriptorSets failed");
	return DescriptorSet(layout, index, ref(), handle);
}

VulkanDescriptorPool::VulkanDescriptorPool(VkDescriptorPool handle, VObjectId id, uint max_sets)
	: VulkanObjectBase(handle, id), m_max_sets(max_sets) {}
VulkanDescriptorPool ::~VulkanDescriptorPool() {
	deferredHandleRelease<VkDescriptorPool, vkDestroyDescriptorPool>();
}

VulkanRenderPass::VulkanRenderPass(VkRenderPass handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}
VulkanRenderPass ::~VulkanRenderPass() {
	// TODO: here we probably don't need defer
	deferredHandleRelease<VkRenderPass, vkDestroyRenderPass>();
}

VulkanPipelineLayout::VulkanPipelineLayout(VkPipelineLayout handle, VObjectId id,
										   vector<PVDescriptorSetLayout> dsls)
	: VulkanObjectBase(handle, id), m_dsls(move(dsls)) {}
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

Ex<PVRenderPass> VulkanRenderPass::create(VDeviceRef device, const RenderPassDesc &desc) {
	static constexpr int max_colors = RenderPassDesc::max_color_attachments;
	array<VkAttachmentDescription, max_colors + 1> attachments;
	array<VkAttachmentReference, max_colors + 1> attachment_refs;

	DASSERT(desc.colors_sync.size() == desc.colors.size());
	DASSERT(desc.depth.hasValue() == desc.depth_sync.hasValue());

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = desc.colors.size();
	subpass.pColorAttachments = attachment_refs.data();

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	for(int i : intRange(desc.colors)) {
		auto &src = desc.colors[i];
		auto &sync = desc.colors_sync[i];
		auto &dst = attachments[i];
		dst.flags = 0;
		dst.format = src.format;
		dst.samples = VkSampleCountFlagBits(src.num_samples);
		dst.loadOp = toVk(sync.load_op);
		dst.storeOp = toVk(sync.store_op);
		dst.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		dst.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		dst.initialLayout = toVk(sync.init_layout);
		dst.finalLayout = toVk(sync.final_layout);
		attachment_refs[i].attachment = i;
		// TODO: is this the right layout?
		attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	int depth_index = -1;
	if(desc.depth) {
		depth_index = desc.colors.size();
		auto &src = *desc.depth;
		auto &sync = *desc.depth_sync;
		auto &dst = attachments[depth_index];
		dst.flags = 0;
		dst.format = src.format;
		dst.samples = VkSampleCountFlagBits(src.num_samples);
		dst.loadOp = toVk(sync.load_op);
		dst.storeOp = toVk(sync.store_op);
		dst.stencilLoadOp = toVk(sync.stencil_load_op);
		dst.stencilStoreOp = toVk(sync.stencil_store_op);
		dst.initialLayout = toVk(sync.init_layout);
		dst.finalLayout = toVk(sync.final_layout);
		attachment_refs[depth_index].attachment = depth_index;
		attachment_refs[depth_index].layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		subpass.pDepthStencilAttachment = attachment_refs.data() + max_colors;

		dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}

	return create(device, cspan(attachments.data(), desc.colors.size()), cspan(&subpass, 1),
				  cspan(&dependency, 1));
}

Ex<PVRenderPass> VulkanRenderPass::create(VDeviceRef device,
										  CSpan<VkAttachmentDescription> attachments,
										  CSpan<VkSubpassDescription> subpasses,
										  CSpan<VkSubpassDependency> dependencies) {
	VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
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

Ex<PVPipelineLayout> VulkanPipeline::createLayout(VDeviceRef device,
												  vector<PVDescriptorSetLayout> dsls) {
	VkPipelineLayoutCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	auto sl_handles = transform<VkDescriptorSetLayout>(dsls);
	ci.setLayoutCount = dsls.size();
	ci.pSetLayouts = sl_handles.data();
	ci.pushConstantRangeCount = 0;
	ci.pPushConstantRanges = nullptr;

	VkPipelineLayout handle;
	if(vkCreatePipelineLayout(device->handle(), &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreatePipelineLayout failed");
	return device->createObject(handle, move(dsls));
}

Ex<PVPipeline> VulkanPipeline::create(VDeviceRef device, VPipelineSetup setup) {
	DASSERT(setup.render_pass);

	vector<DescriptorBindingInfo> descr_bindings;
	for(auto &stage : setup.stages) {
		auto stage_bindings = stage.shader_module->descriptorBindingInfos();
		if(descr_bindings.empty())
			descr_bindings = stage_bindings;
		else
			descr_bindings = DescriptorBindingInfo::merge(descr_bindings, stage_bindings);
	}
	auto descr_sets = DescriptorBindingInfo::divideSets(descr_bindings);
	vector<PVDescriptorSetLayout> dsls;
	dsls.reserve(descr_sets.size());
	for(auto bindings : descr_sets)
		dsls.emplace_back(EX_PASS(device->getDSL(bindings)));
	auto pipeline_layout = EX_PASS(createLayout(device, move(dsls)));

	array<VkPipelineShaderStageCreateInfo, count<VShaderStage>> stages_ci;
	for(int i : intRange(setup.stages)) {
		DASSERT(setup.stages[i].shader_module);
		stages_ci[i] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
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