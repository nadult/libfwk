// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_pipeline.h"

#include "fwk/index_range.h"
#include "fwk/sys/assert.h"
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

VulkanRenderPass::VulkanRenderPass(VkRenderPass handle, VObjectId id, const RenderPassDesc &desc)
	: VulkanObjectBase(handle, id), m_num_color_attachments(desc.colors.size()) {}
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
	static constexpr int max_colors = VulkanLimits::max_color_attachments;
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

	VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	ci.attachmentCount = desc.colors.size();
	ci.pAttachments = attachments.data();
	ci.subpassCount = 1;
	ci.pSubpasses = &subpass;
	ci.dependencyCount = 1;
	ci.pDependencies = &dependency;

	VkRenderPass handle;
	if(vkCreateRenderPass(device->handle(), &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateRenderPass failed");
	return device->createObject(handle, desc);
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
	for(auto &shader_module : setup.shader_modules) {
		DASSERT(shader_module);
		auto stage_bindings = shader_module->descriptorBindingInfos();
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
	for(int i : intRange(setup.shader_modules)) {
		auto &shader_module = setup.shader_modules[i];
		DASSERT(shader_module);
		stages_ci[i] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stages_ci[i].stage = toVk(shader_module->stage());
		stages_ci[i].module = shader_module;
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

	FRect view_rect(setup.viewport.rect);
	VkViewport viewport{view_rect.x(),
						view_rect.y(),
						view_rect.width(),
						view_rect.height(),
						setup.viewport.min_depth,
						setup.viewport.max_depth};

	IRect scissor_rect = setup.scissor.orElse(setup.viewport.rect);
	VkRect2D scissor = {{scissor_rect.x(), scissor_rect.y()},
						{uint(scissor_rect.width()), uint(scissor_rect.height())}};

	VkPipelineViewportStateCreateInfo viewport_state_ci{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	viewport_state_ci.viewportCount = 1;
	viewport_state_ci.pViewports = &viewport;
	viewport_state_ci.scissorCount = 1;
	viewport_state_ci.pScissors = &scissor;

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
	int num_color_attachments = setup.render_pass->numColorAttachments();
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
	if(vkCreateGraphicsPipelines(device->handle(), device->pipelineCache(), 1, &ci, nullptr,
								 &handle) != VK_SUCCESS)
		return ERROR("vkCreateGraphicsPipelines failed");
	return device->createObject(handle, setup.render_pass, pipeline_layout);
}
}