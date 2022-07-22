// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_pipeline.h"

#include "fwk/index_range.h"
#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_shader.h"

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

VulkanSampler::VulkanSampler(VkSampler handle, VObjectId id, const VSamplerSetup &params)
	: VulkanObjectBase(handle, id), m_params(params) {}
VulkanSampler::~VulkanSampler() { deferredRelease<vkDestroySampler>(m_handle); }

VDescriptorSet::Assigner &VDescriptorSet::Assigner::operator()(int binding_index, PVBuffer buffer,
															   Maybe<BufferRange> range,
															   VDescriptorType type) {
	PASSERT(size < max_assignments);
	auto &info = additional_data[size].buffer_info;
	auto &write = assignments[size++];
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;
	write.dstSet = set_handle;
	write.dstBinding = binding_index;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = toVk(type);
	write.pBufferInfo = &info;
	write.pImageInfo = nullptr;
	write.pTexelBufferView = nullptr;

	info.buffer = buffer;
	if(range) {
		info.offset = range->offset;
		info.range = range->size;
	} else {
		info.offset = 0;
		info.range = VK_WHOLE_SIZE;
	}
	return *this;
}

VDescriptorSet::Assigner &VDescriptorSet::Assigner::operator()(int binding_index, VSpan span,
															   VDescriptorType type) {
	return operator()(binding_index, span.buffer, BufferRange{span.offset, span.size}, type);
}

VDescriptorSet::Assigner &VDescriptorSet::Assigner::operator()(int binding_index, PVSampler sampler,
															   PVImageView image) {
	PASSERT(size < max_assignments);
	auto &info = additional_data[size].image_info;
	auto &write = assignments[size++];
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;
	write.dstSet = set_handle;
	write.dstBinding = binding_index;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = toVk(VDescriptorType::combined_image_sampler);
	write.pImageInfo = &info;
	write.pBufferInfo = nullptr;
	write.pTexelBufferView = nullptr;
	info.imageView = image;
	info.sampler = sampler;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	return *this;
}

void VDescriptorSet::Assigner::performAssignment() {
	vkUpdateDescriptorSets(device_handle, size, assignments.data(), 0, nullptr);
}

VulkanRenderPass::VulkanRenderPass(VkRenderPass handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}
VulkanRenderPass ::~VulkanRenderPass() {
	// TODO: here we probably don't need defer
	deferredRelease<vkDestroyRenderPass>(m_handle);
}

u32 VulkanRenderPass::hashConfig(CSpan<VColorAttachment> colors, const VDepthAttachment *depth) {
	auto out = fwk::hash(colors);
	if(depth)
		out = combineHash(out, fwk::hash(*depth));
	return out;
}

PVRenderPass VulkanRenderPass::create(VDeviceRef device, CSpan<VColorAttachment> colors,
									  Maybe<VDepthAttachment> depth) {

	array<VkAttachmentDescription, max_colors + 1> attachments;
	array<VkAttachmentReference, max_colors + 1> attachment_refs;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colors.size();
	subpass.pColorAttachments = attachment_refs.data();

	// TODO: what's the point of that?
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	for(int i : intRange(colors)) {
		auto &src = colors[i];
		auto &dst = attachments[i];
		dst.flags = 0;
		dst.format = src.format;
		dst.samples = VkSampleCountFlagBits(src.num_samples);
		dst.loadOp = toVk(src.sync.load_op);
		dst.storeOp = toVk(src.sync.store_op);
		dst.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		dst.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		dst.initialLayout = toVk(src.sync.initial_layout);
		dst.finalLayout = toVk(src.sync.final_layout);
		attachment_refs[i].attachment = i;
		// TODO: is this the right layout?
		attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	int depth_index = -1;
	if(depth) {
		depth_index = colors.size();
		auto &src = *depth;
		auto &dst = attachments[depth_index];
		dst.flags = 0;
		dst.format = toVk(src.format);
		dst.samples = VkSampleCountFlagBits(src.num_samples);
		dst.loadOp = toVk(src.sync.load_op);
		dst.storeOp = toVk(src.sync.store_op);
		dst.stencilLoadOp = toVk(src.sync.stencil_load_op);
		dst.stencilStoreOp = toVk(src.sync.stencil_store_op);
		dst.initialLayout = toVk(src.sync.initial_layout);
		dst.finalLayout = toVk(src.sync.final_layout);
		attachment_refs[depth_index].attachment = depth_index;
		attachment_refs[depth_index].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		subpass.pDepthStencilAttachment = attachment_refs.data() + colors.size();

		dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask |=
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
									VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	}

	VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	ci.attachmentCount = colors.size() + (depth ? 1 : 0);
	ci.pAttachments = attachments.data();
	ci.subpassCount = 1;
	ci.pSubpasses = &subpass;
	ci.dependencyCount = 1;
	ci.pDependencies = &dependency;

	VkRenderPass handle;
	FWK_VK_CALL(vkCreateRenderPass, device->handle(), &ci, nullptr, &handle);
	auto out = device->createObject(handle);
	out->m_colors = colors;
	out->m_depth = depth;

	return out;
}

VulkanPipelineLayout::VulkanPipelineLayout(VkPipelineLayout handle, VObjectId id,
										   vector<VDSLId> dsls)
	: VulkanObjectBase(handle, id), m_dsls(move(dsls)) {}
VulkanPipelineLayout ::~VulkanPipelineLayout() {
	// TODO: do we really need deferred?
	deferredRelease<vkDestroyPipelineLayout>(m_handle);
}

PVPipelineLayout VulkanPipelineLayout::create(VDeviceRef device, vector<VDSLId> dsls) {
	VkPipelineLayoutCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	// TODO: array
	auto sl_handles = transform(dsls, [&](VDSLId id) { return device->handle(id); });
	ci.setLayoutCount = dsls.size();
	ci.pSetLayouts = sl_handles.data();
	ci.pushConstantRangeCount = 0;
	ci.pPushConstantRanges = nullptr;

	VkPipelineLayout handle;
	FWK_VK_CALL(vkCreatePipelineLayout, device->handle(), &ci, nullptr, &handle);
	return device->createObject(handle, move(dsls));
}

VulkanPipeline::VulkanPipeline(VkPipeline handle, VObjectId id, PVRenderPass rp,
							   PVPipelineLayout lt)
	: VulkanObjectBase(handle, id), m_render_pass(rp), m_layout(lt) {}
VulkanPipeline::~VulkanPipeline() { deferredRelease<vkDestroyPipeline>(m_handle); }

Ex<PVPipeline> VulkanPipeline::create(VDeviceRef device, VPipelineSetup setup) {
	DASSERT(setup.render_pass);
	DASSERT(setup.dynamic_state & VDynamic::viewport);
	DASSERT(setup.dynamic_state & VDynamic::scissor);

	vector<DescriptorBindingInfo> descr_bindings;
	for(auto &shader_module : setup.shader_modules) {
		DASSERT(shader_module);
		auto stage_bindings = shader_module->descriptorBindingInfos();
		if(descr_bindings.empty())
			descr_bindings = stage_bindings;
		else
			descr_bindings = DescriptorBindingInfo::merge(descr_bindings, stage_bindings);
	}
	auto descr_sets = DescriptorBindingInfo::divideSets(descr_bindings);
	vector<VDSLId> dsls;
	dsls.reserve(descr_sets.size());
	for(auto bindings : descr_sets)
		dsls.emplace_back(device->getDSL(bindings));
	auto pipeline_layout = device->getPipelineLayout(dsls);

	array<VkPipelineShaderStageCreateInfo, count<VShaderStage>> stages_ci;
	for(int i : intRange(setup.shader_modules)) {
		auto &shader_module = setup.shader_modules[i];
		DASSERT(shader_module);
		stages_ci[i] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stages_ci[i].stage = toVk(shader_module->stage());
		stages_ci[i].module = shader_module;
		stages_ci[i].pName = "main";
	}

	auto vertex_bindings = transform(setup.vertex_bindings, [](const VVertexBinding &desc) {
		VkVertexInputBindingDescription out;
		out.binding = desc.index;
		out.inputRate = desc.input_rate == VertexInputRate::vertex ? VK_VERTEX_INPUT_RATE_VERTEX :
																	   VK_VERTEX_INPUT_RATE_INSTANCE;
		out.stride = desc.stride;
		return out;
	});

	auto vertex_attribs = transform(setup.vertex_attribs, [](const VVertexAttrib &desc) {
		VkVertexInputAttributeDescription out;
		out.binding = desc.binding_index;
		out.format = desc.format;
		out.location = desc.location_index;
		out.offset = desc.offset;
		return out;
	});

	VkPipelineVertexInputStateCreateInfo vertex_input_ci{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vertex_input_ci.vertexBindingDescriptionCount = vertex_bindings.size();
	vertex_input_ci.pVertexBindingDescriptions = vertex_bindings.data();
	vertex_input_ci.vertexAttributeDescriptionCount = vertex_attribs.size();
	vertex_input_ci.pVertexAttributeDescriptions = vertex_attribs.data();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
	input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_ci.topology = toVk(setup.raster.primitiveTopology());
	input_assembly_ci.primitiveRestartEnable =
		!!(setup.raster.flags() & VRasterFlag::primitive_restart);

	VkPipelineViewportStateCreateInfo viewport_state_ci{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	viewport_state_ci.viewportCount = 1;
	viewport_state_ci.pViewports = nullptr;
	viewport_state_ci.scissorCount = 1;
	viewport_state_ci.pScissors = nullptr;

	VkPipelineRasterizationStateCreateInfo rasterizer_ci{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rasterizer_ci.depthClampEnable = !!(setup.depth.flags() & VDepthFlag::clamp);
	rasterizer_ci.rasterizerDiscardEnable = !!(setup.raster.flags() & VRasterFlag::discard);
	rasterizer_ci.polygonMode = toVk(setup.raster.polygonMode());
	rasterizer_ci.lineWidth = setup.raster.line_width;
	rasterizer_ci.cullMode = toVk(setup.raster.cullMode());
	rasterizer_ci.frontFace = toVk(setup.raster.frontFace());
	rasterizer_ci.depthBiasEnable = !!(setup.depth.flags() & VDepthFlag::bias);
	rasterizer_ci.depthBiasConstantFactor = setup.depth.bias.constant_factor;
	rasterizer_ci.depthBiasClamp = setup.depth.bias.clamp;
	rasterizer_ci.depthBiasSlopeFactor = setup.depth.bias.slope_factor;

	VkPipelineDepthStencilStateCreateInfo depth_stencil_ci{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	depth_stencil_ci.depthTestEnable = !!(setup.depth.flags() & VDepthFlag::test);
	depth_stencil_ci.depthWriteEnable = !!(setup.depth.flags() & VDepthFlag::write);
	depth_stencil_ci.depthCompareOp = toVk(setup.depth.compareOp());
	depth_stencil_ci.depthBoundsTestEnable = !!(setup.depth.flags() & VDepthFlag::bounds_test);
	depth_stencil_ci.minDepthBounds = setup.depth.bounds.min;
	depth_stencil_ci.maxDepthBounds = setup.depth.bounds.max;

	VkPipelineMultisampleStateCreateInfo multisampling_ci{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	multisampling_ci.sampleShadingEnable = VK_FALSE;
	multisampling_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling_ci.minSampleShading = 1.0f; // Optional
	multisampling_ci.pSampleMask = nullptr; // Optional
	multisampling_ci.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling_ci.alphaToOneEnable = VK_FALSE; // Optional

	array<VkPipelineColorBlendAttachmentState, VulkanLimits::max_color_attachments>
		blend_attachments;
	int num_color_attachments = setup.render_pass->colors().size();
	DASSERT_LE(setup.blending.attachments.size(), num_color_attachments);

	// TODO: check num modes, fill missing?
	for(int i = 0; i < setup.blending.attachments.size(); i++) {
		auto &src = setup.blending.attachments[i];
		auto &dst = blend_attachments[i];
		if(src.enabled()) {
			dst.blendEnable = VK_TRUE;
			dst.srcColorBlendFactor = toVk(src.srcColor());
			dst.dstColorBlendFactor = toVk(src.dstColor());
			dst.colorBlendOp = toVk(src.colorOp());
			dst.srcAlphaBlendFactor = toVk(src.srcAlpha());
			dst.dstAlphaBlendFactor = toVk(src.dstAlpha());
			dst.alphaBlendOp = toVk(src.alphaOp());
		} else {
			dst = {};
		}
		dst.colorWriteMask = toVk(src.writeMask());
	}
	for(int i = setup.blending.attachments.size(); i < num_color_attachments; i++) {
		auto &dst = blend_attachments[i];
		dst = {};
		dst.colorWriteMask = toVk(all<VColorComponent>);
	}

	VkPipelineColorBlendStateCreateInfo blending_ci{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	blending_ci.logicOpEnable = VK_FALSE;
	blending_ci.logicOp = VK_LOGIC_OP_COPY;
	blending_ci.attachmentCount = num_color_attachments;
	blending_ci.pAttachments = blend_attachments.data();
	for(int i : intRange(4))
		blending_ci.blendConstants[i] = setup.blending.constant[i];

	VkDynamicState dynamic_states[count<VDynamic>];
	int num_dynamic = 0;
	for(auto dynamic : setup.dynamic_state)
		dynamic_states[num_dynamic++] = toVk(dynamic);
	VkPipelineDynamicStateCreateInfo dynamic_ci{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dynamic_ci.dynamicStateCount = num_dynamic;
	dynamic_ci.pDynamicStates = dynamic_states;

	VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	ci.stageCount = setup.shader_modules.size();
	ci.pStages = stages_ci.data();
	ci.pVertexInputState = &vertex_input_ci;
	ci.pInputAssemblyState = &input_assembly_ci;
	ci.pViewportState = &viewport_state_ci;
	ci.pRasterizationState = &rasterizer_ci;
	ci.pMultisampleState = &multisampling_ci;
	ci.pDepthStencilState = &depth_stencil_ci;
	ci.pColorBlendState = &blending_ci;
	ci.pDynamicState = num_dynamic > 0 ? &dynamic_ci : nullptr;
	ci.layout = pipeline_layout;
	ci.renderPass = setup.render_pass;
	ci.subpass = 0;
	ci.basePipelineHandle = VK_NULL_HANDLE;
	ci.basePipelineIndex = -1;

	VkPipeline handle;
	FWK_VK_EXPECT_CALL(vkCreateGraphicsPipelines, device->handle(), device->pipelineCache(), 1, &ci,
					   nullptr, &handle);
	return device->createObject(handle, setup.render_pass, pipeline_layout);
}

Ex<PVPipeline> VulkanPipeline::create(VDeviceRef device, const VComputePipelineSetup &setup) {
	DASSERT(setup.compute_module);

	auto descr_bindings = setup.compute_module->descriptorBindingInfos();
	auto descr_sets = DescriptorBindingInfo::divideSets(descr_bindings);
	vector<VDSLId> dsls;
	dsls.reserve(descr_sets.size());
	for(auto bindings : descr_sets)
		dsls.emplace_back(device->getDSL(bindings));
	auto pipeline_layout = device->getPipelineLayout(dsls);

	VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ci.stage.stage = toVk(setup.compute_module->stage());
	ci.stage.module = setup.compute_module;
	ci.stage.pName = "main";
	ci.layout = pipeline_layout;
	ci.basePipelineIndex = -1;

	VkPipeline handle;
	FWK_VK_EXPECT_CALL(vkCreateComputePipelines, device->handle(), device->pipelineCache(), 1, &ci,
					   nullptr, &handle);
	auto out = device->createObject(handle, PVRenderPass(), pipeline_layout);
	out->m_bind_point = VBindPoint::compute;
	return out;
}
}