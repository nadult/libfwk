// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/drawing.h"

#include "fwk/gfx/colored_triangle.h"
#include "fwk/hash_map.h"
#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_command_queue.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_shader.h"

namespace fwk {

SimpleMaterial::SimpleMaterial(PVImageView texture, IColor color, DrawingFlags drawing_flags,
							   BlendingMode blending_mode)
	: m_texture(texture), m_color(color), m_drawing_flags(drawing_flags),
	  m_blending_mode(blending_mode) {}
SimpleMaterial::SimpleMaterial(IColor color, DrawingFlags drawing_flags, BlendingMode blending_mode)
	: m_color(color), m_drawing_flags(drawing_flags), m_blending_mode(blending_mode) {}

FWK_ORDER_BY_DEF(SimpleMaterial, m_texture, m_color, m_drawing_flags, m_blending_mode);
FWK_ORDER_BY_DEF(SimplePipelineSetup, flags, blending_mode, primitive_topo);

void SimpleDrawCall::getVertexBindings(vector<VVertexAttrib> &attribs,
									   vector<VVertexBinding> &bindings) {
	insertBack(attribs, {vertexAttrib<float3>(0, 0), vertexAttrib<float2>(1, 1),
						 vertexAttrib<IColor>(2, 2)});
	insertBack(bindings,
			   {vertexBinding<float3>(0), vertexBinding<float2>(1), vertexBinding<IColor>(2)});
}

//	vec3 normal = normalize(cross(dFdx(v_tpos), dFdy(v_tpos)));
//	float shade = abs(dot(normal, vec3(0, 0, 1))) * 0.5 + 0.5;

const char *simple_vsh = R"(
#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_tex_coord;

layout(binding = 0) readonly restrict buffer buf0_ { mat4 proj_view_matrices[]; };

void main() {
	gl_Position = proj_view_matrices[gl_InstanceIndex] * vec4(in_pos, 1.0);
	out_color = in_color;
	out_tex_coord = in_tex_coord;
}
)";

const char *simple_fsh = R"(
#version 450

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_tex_coord;

layout(set = 1, binding = 0) uniform sampler2D tex_sampler;

void main() {
	// TODO: optional texturing
	out_color = in_color * texture(tex_sampler, in_tex_coord);
}
)";

static Ex<PVPipeline> makePipeline(ShaderCompiler &compiler, VulkanDevice &device,
								   PVRenderPass render_pass, SimpleBlendingMode simple_bm,
								   SimpleDrawingFlags flags, VPrimitiveTopology topo) {
	VPipelineSetup setup;

	// TODO: use shader compiler
	setup.shader_modules = EX_PASS(VulkanShaderModule::compile(
		compiler, device.ref(),
		{{VShaderStage::vertex, simple_vsh}, {VShaderStage::fragment, simple_fsh}}));
	setup.render_pass = render_pass;
	SimpleDrawCall::getVertexBindings(setup.vertex_attribs, setup.vertex_bindings);
	setup.raster = VRasterSetup(topo, VPolygonMode::fill, none);

	if(simple_bm != SimpleBlendingMode::disabled) {
		VBlendingMode blending_mode;
		if(simple_bm == SimpleBlendingMode::additive)
			blending_mode = {VBlendFactor::one, VBlendFactor::one};
		else if(simple_bm == SimpleBlendingMode::normal)
			blending_mode = {VBlendFactor::src_alpha, VBlendFactor::one_minus_src_alpha};
		setup.blending.attachments = {{blending_mode}};
	}

	return VulkanPipeline::create(device.ref(), setup);
}

Ex<vector<PVPipeline>> SimpleDrawCall::makePipelines(ShaderCompiler &compiler, VulkanDevice &device,
													 PVRenderPass render_pass,
													 CSpan<SimplePipelineSetup> setups) {
	vector<PVPipeline> pipelines;
	pipelines.reserve(setups.size());
	for(auto &setup : setups) {
		auto pipeline = EX_PASS(device.getCachedPipeline(compiler, makePipeline, render_pass,
														 setup.blending_mode, setup.flags,
														 setup.primitive_topo));
		pipelines.emplace_back(move(pipeline));
	}
	return pipelines;
}

void SimpleDrawCall::render(VulkanDevice &device) {
	if(!instances || !vertices)
		return;
	DASSERT(pipelines);
	auto sampler = device.getSampler({VTexFilter::linear, VTexFilter::linear});

	auto &cmds = device.cmdQueue();
	cmds.bind(pipelines.front());
	cmds.bindDS(0).set(0, instance_matrices);

	int prev_pipeline_idx = -1;
	PVImageView prev_tex;
	cmds.bindDS(1).set(0, {{sampler, prev_tex}});

	cmds.bindVertices(0, vertices, tex_coords, colors);
	if(indices)
		cmds.bindIndices(indices);

	uint instance_id = 0;
	for(auto &instance : instances) {
		if(instance.pipeline_index != prev_pipeline_idx) {
			cmds.bind(pipelines[instance.pipeline_index]);
			prev_pipeline_idx = instance.pipeline_index;

			if(instance.texture != prev_tex) {
				// TODO: we need as many DSes as we have different textures
				cmds.bindDS(1).set(0, {{sampler, instance.texture}});
				prev_tex = instance.texture;
			}
		}

		if(indices)
			cmds.drawIndexed(instance.num_vertices, 1, instance.first_index, instance_id++);
		else
			cmds.draw(instance.num_vertices, 1, instance.first_index, instance_id++);
	}
}

/*
Ex<SimpleVertexArray> SimpleVertexArray::make(VulkanDevice &device, CSpan<float3> vertex_data,
											  CSpan<IColor> color_data, CSpan<float2> tex_data,
											  CSpan<float3> normal_data, CSpan<u32> index_data,
											  VPrimitiveTopology topo, VMemoryUsage mem_usage) {
	SimpleVertexArray out;
	auto usage = VBufferUsage::vertex_buffer | VBufferUsage::transfer_src;
	out.vertices = EX_PASS(VulkanBuffer::createAndUpload(device, vertex_data, usage, mem_usage));
	out.colors = EX_PASS(VulkanBuffer::createAndUpload(device, color_data, usage, mem_usage));
	out.tex_coords = EX_PASS(VulkanBuffer::createAndUpload(device, tex_data, usage, mem_usage));
	out.normals = EX_PASS(VulkanBuffer::createAndUpload(device, normal_data, usage, mem_usage));

	usage = VBufferUsage::index_buffer | VBufferUsage::transfer_src;
	out.indices = EX_PASS(VulkanBuffer::createAndUpload(device, index_data, usage, mem_usage));
	out.topology = topo;
	return out;
}

Ex<SimpleVertexArray> SimpleVertexArray::make(VulkanDevice &device, CSpan<ColoredTriangle> tris,
											  VMemoryUsage mem_usage) {
	vector<float3> positions;
	vector<IColor> colors;

	positions.reserve(tris.size() * 3);
	colors.reserve(tris.size() * 3);
	for(auto &tri : tris)
		for(int i = 0; i < 3; i++) {
			positions.emplace_back(tri[i]);
			colors.emplace_back(tri.colors[i]);
		}
	return make(device, positions, colors, {}, {}, {}, VPrimitiveTopology::triangle_list,
				mem_usage);
}

SimpleDrawCall::SimpleDrawCall(SimpleVertexArray vertex_array, const SimpleMaterial &material,
							   Matrix4 matrix, Maybe<FBox> transformed_bbox)
	: matrix(matrix), transformed_bbox(transformed_bbox), material(material),
	  vertex_array(move(vertex_array)) {}

FWK_COPYABLE_CLASS_IMPL(SimpleDrawCall);*/

}
