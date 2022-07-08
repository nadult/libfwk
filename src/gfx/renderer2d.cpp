// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/renderer2d.h"
#include "fwk/gfx/image.h"
#include "fwk/hash_map.h"
#include "fwk/io/xml.h"
#include "fwk/math/constants.h"
#include "fwk/math/rotation.h"
#include "fwk/sys/on_fail.h"

#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_render_graph.h"
#include "fwk/vulkan/vulkan_shader.h"

// Czy chcemy, ¿eby Renderer2D bazowa³ na ElementBufferze ?
// Czy jest sens to robiæ teraz ? nie, zróbmy tylko, ¿eby dzia³a³o, nawet bez optymalizacji?

namespace fwk {

const char *vsh_2d = R"(
#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_tex_coord;

layout(binding = 0) readonly restrict buffer buf0_ { mat4 proj_view_matrices[]; };

void main() {
	gl_Position = proj_view_matrices[gl_InstanceIndex] * vec4(in_pos, 0.0, 1.0);
	out_color = in_color;
	out_tex_coord = in_tex_coord;
}
)";

const char *fsh_2d = R"(
#version 450

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_tex_coord;

layout(set = 1, binding = 0) uniform sampler2D tex_sampler;

void main() {
	out_color = in_color * texture(tex_sampler, in_tex_coord);
}
)";

Renderer2D::Renderer2D(const IRect &viewport, Orient2D orient)
	: MatrixStack(projectionMatrix2D(viewport, orient), viewMatrix2D(viewport, float2(0, 0))),
	  m_viewport(viewport), m_current_scissor_rect(-1) {}

Renderer2D::~Renderer2D() = default;

void Renderer2D::setViewPos(const float2 &view_pos) {
	MatrixStack::setViewMatrix(viewMatrix2D(m_viewport, view_pos));
}

Renderer2D::DrawChunk &Renderer2D::allocChunk(int num_verts) {
	if(!m_chunks || m_chunks.back().positions.size() + num_verts > 65535)
		m_chunks.emplace_back();
	return m_chunks.back();
}

Renderer2D::Element &Renderer2D::makeElement(DrawChunk &chunk, PrimitiveType primitive_type,
											 PVImageView texture, BlendingMode bm) {
	// TODO: merging won't work for triangle strip (have to add some more indices)
	auto &elems = chunk.elements;

	if(!elems || elems.back().primitive_type != primitive_type || elems.back().texture != texture ||
	   elems.back().blending_mode != bm || fullMatrix() != elems.back().matrix ||
	   m_current_scissor_rect != elems.back().scissor_rect_id)
		elems.emplace_back(Element{fullMatrix(), move(texture), chunk.indices.size(), 0,
								   m_current_scissor_rect, primitive_type, bm});
	return elems.back();
}

void Renderer2D::addFilledRect(const FRect &rect, const FRect &tex_rect, CSpan<FColor, 4> colors,
							   const SimpleMaterial &material) {
	addQuads(rect.corners(), tex_rect.corners(), colors, material);
}

void Renderer2D::addFilledRect(const FRect &rect, const FRect &tex_rect,
							   const SimpleMaterial &material) {
	addQuads(rect.corners(), tex_rect.corners(), {}, material);
}

void Renderer2D::addFilledEllipse(float2 center, float2 size, FColor color, int num_tris) {
	PASSERT(num_tris >= 3);
	auto &chunk = allocChunk(num_tris + 1);
	auto &elem = makeElement(chunk, PrimitiveType::triangles, {});
	int idx = chunk.positions.size();

	auto ang_mul = pi * 2.0f / float(num_tris);
	for(int n = 0; n < num_tris; n++)
		chunk.positions.emplace_back(angleToVector(float(n) * ang_mul) * size + center);
	chunk.positions.emplace_back(center);

	for(int n = 0; n < num_tris; n++) {
		int next = n == num_tris - 1 ? 0 : n + 1;
		insertBack(chunk.indices, {idx + num_tris, idx + n, idx + next});
	}

	chunk.tex_coords.resize(chunk.positions.size(), float2());
	chunk.colors.resize(chunk.positions.size(), IColor(color));
	elem.num_indices += num_tris * 3;
}

void Renderer2D::addEllipse(float2 center, float2 size, FColor color, int num_edges) {
	auto &chunk = allocChunk(num_edges + 1);
	auto &elem = makeElement(chunk, PrimitiveType::lines, {});

	auto ang_mul = pi * 2.0f / float(num_edges);
	int idx = chunk.positions.size();
	for(int n = 0; n < num_edges; n++) {
		chunk.positions.emplace_back(angleToVector(float(n) * ang_mul) * size + center);
		int next = n == num_edges - 1 ? 0 : n + 1;
		insertBack(chunk.indices, {idx + n, idx + next});
	}
	chunk.tex_coords.resize(chunk.positions.size(), float2());
	chunk.colors.resize(chunk.positions.size(), IColor(color));
	elem.num_indices += num_edges * 2;
}

void Renderer2D::addRect(const FRect &rect, FColor color) {
	auto &chunk = allocChunk(4);
	Element &elem = makeElement(chunk, PrimitiveType::lines, {});
	int vertex_offset = chunk.positions.size();
	chunk.appendVertices(rect.corners(), {}, {}, color);

	const int num_indices = 8;
	int indices[num_indices] = {0, 1, 1, 2, 2, 3, 3, 0};
	for(int i = 0; i < num_indices; i++)
		chunk.indices.emplace_back(vertex_offset + indices[i]);
	elem.num_indices += num_indices;
}

void Renderer2D::addLine(const float2 &p1, const float2 &p2, FColor color) {
	auto &chunk = allocChunk(2);
	Element &elem = makeElement(chunk, PrimitiveType::lines, {});
	int vertex_offset = chunk.positions.size();
	chunk.appendVertices({p1, p2}, {}, {}, color);

	insertBack(chunk.indices, {vertex_offset, vertex_offset + 1});
	elem.num_indices += 2;
}

void Renderer2D::DrawChunk::appendVertices(CSpan<float2> positions_, CSpan<float2> tex_coords_,
										   CSpan<FColor> colors_, FColor mat_color) {
	DASSERT(colors.size() == positions.size() || !colors);
	DASSERT(tex_coords.size() == positions.size() || !tex_coords);

	insertBack(positions, positions_);
	if(colors_)
		for(auto col : colors_)
			colors.emplace_back(col * mat_color);
	else
		colors.resize(positions.size(), IColor(mat_color));
	if(tex_coords_)
		insertBack(tex_coords, tex_coords_);
	else
		tex_coords.resize(positions.size(), float2());
}

void Renderer2D::addLines(CSpan<float2> pos, CSpan<FColor> color, FColor mat_color) {
	DASSERT(pos.size() % 2 == 0);

	auto &chunk = allocChunk(pos.size());
	Element &elem = makeElement(chunk, PrimitiveType::lines, {});

	int vertex_offset = chunk.positions.size();
	chunk.appendVertices(pos, {}, color, mat_color);
	for(int n = 0; n < pos.size(); n++)
		chunk.indices.emplace_back(vertex_offset + n);
	elem.num_indices += pos.size();
}

void Renderer2D::addQuads(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor> color,
						  const SimpleMaterial &material) {
	DASSERT(pos.size() % 4 == 0);

	auto &chunk = allocChunk(pos.size());
	Element &elem =
		makeElement(chunk, PrimitiveType::triangles, material.texture(), material.blendingMode());
	int vertex_offset = chunk.positions.size();
	int num_quads = pos.size() / 4;

	chunk.appendVertices(pos, tex_coord, color, material.color());
	for(int n = 0; n < num_quads; n++) {
		int inds[6] = {0, 1, 2, 0, 2, 3};
		for(int i = 0; i < 6; i++)
			chunk.indices.emplace_back(vertex_offset + n * 4 + inds[i]);
	}
	elem.num_indices += num_quads * 6;
}

void Renderer2D::addTris(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor> color,
						 const SimpleMaterial &material) {
	DASSERT(pos.size() % 3 == 0);

	int num_tris = pos.size() / 3;

	auto &chunk = allocChunk(pos.size());
	Element &elem =
		makeElement(chunk, PrimitiveType::triangles, material.texture(), material.blendingMode());
	int vertex_offset = chunk.positions.size();
	chunk.appendVertices(pos, tex_coord, color, material.color());

	for(int n = 0; n < num_tris; n++) {
		int inds[3] = {0, 1, 2};
		for(int i = 0; i < 3; i++)
			chunk.indices.emplace_back(vertex_offset + n * 3 + inds[i]);
	}
	elem.num_indices += num_tris * 3;
}

Maybe<IRect> Renderer2D::scissorRect() const {
	if(m_current_scissor_rect != -1)
		return m_scissor_rects[m_current_scissor_rect];
	return none;
}

void Renderer2D::setScissorRect(Maybe<IRect> rect) {
	if(!rect) {
		m_current_scissor_rect = -1;
		return;
	}
	if(!m_scissor_rects || m_current_scissor_rect == -1 ||
	   m_scissor_rects[m_current_scissor_rect] != *rect) {
		m_scissor_rects.emplace_back(*rect);
		m_current_scissor_rect = m_scissor_rects.size() - 1;
	}
}

void Renderer2D::clear() {
	m_chunks.clear();
	m_scissor_rects.clear();
	m_current_scissor_rect = -1;
}

auto Renderer2D::makeVulkanPipelines(VDeviceRef device, VColorAttachment color_attachment)
	-> Ex<VulkanPipelines> {
	VPipelineSetup setup;
	setup.shader_modules = EX_PASS(VulkanShaderModule::compile(
		device, {{VShaderStage::vertex, vsh_2d}, {VShaderStage::fragment, fsh_2d}}));
	setup.render_pass = EX_PASS(device->getRenderPass({color_attachment}));
	setup.vertex_bindings = {
		{vertexBinding<float2>(0), vertexBinding<IColor>(1), vertexBinding<float2>(2)}};
	setup.vertex_attribs = {
		{vertexAttrib<float2>(0, 0), vertexAttrib<IColor>(1, 1), vertexAttrib<float2>(2, 2)}};

	VBlendingMode additive_blend(VBlendFactor::one, VBlendFactor::one);
	VBlendingMode normal_blend(VBlendFactor::src_alpha, VBlendFactor::one_minus_src_alpha);

	VulkanPipelines out;
	out.pipelines.resize(4);
	setup.raster = VRasterSetup(VPrimitiveTopology::triangle_list, VPolygonMode::fill, none);
	setup.blending.attachments = {{normal_blend}};
	out.pipelines[0] = EX_PASS(VulkanPipeline::create(device, setup));
	setup.blending.attachments = {{additive_blend}};
	out.pipelines[1] = EX_PASS(VulkanPipeline::create(device, setup));

	setup.raster = VRasterSetup(VPrimitiveTopology::line_list, VPolygonMode::fill, none);
	setup.blending.attachments = {{normal_blend}};
	out.pipelines[2] = EX_PASS(VulkanPipeline::create(device, setup));
	setup.blending.attachments = {{additive_blend}};
	out.pipelines[3] = EX_PASS(VulkanPipeline::create(device, setup));

	auto white_image = EX_PASS(VulkanImage::create(device, {VK_FORMAT_R8G8B8A8_UNORM, {4, 4}, 1}));
	out.white = EX_PASS(VulkanImageView::create(device, white_image));

	Image img_data({4, 4}, ColorId::white);
	EXPECT(device->renderGraph() << CmdUploadImage(img_data, white_image));

	VSamplerSetup sampler{VTexFilter::linear, VTexFilter::linear};
	out.sampler = EX_PASS(device->createSampler(sampler));

	return out;
}

Ex<Renderer2D::DrawCall> Renderer2D::genDrawCall(VDeviceRef device, const VulkanPipelines &ctx) {
	auto &pipes = ctx.pipelines;

	uint vertex_pos = 0;
	uint index_pos = 0;
	uint num_elements = 0;

	DrawCall dc;

	for(auto &chunk : m_chunks) {
		vertex_pos += chunk.positions.size() * sizeof(chunk.positions[0]) +
					  chunk.colors.size() * sizeof(chunk.colors[0]) +
					  chunk.tex_coords.size() * sizeof(chunk.tex_coords[0]);
		index_pos += chunk.indices.size() * sizeof(chunk.indices[0]);
		num_elements += chunk.elements.size();
	}

	dc.vbuffer = EX_PASS(VulkanBuffer::create(
		device, vertex_pos, VBufferUsage::vertex_buffer | VBufferUsage::transfer_dst,
		VMemoryUsage::frame));
	dc.ibuffer = EX_PASS(VulkanBuffer::create(
		device, index_pos, VBufferUsage::index_buffer | VBufferUsage::transfer_dst,
		VMemoryUsage::frame));
	dc.matrix_buffer = EX_PASS(VulkanBuffer::create<Matrix4>(
		device, num_elements, VBufferUsage::storage_buffer | VBufferUsage::transfer_dst,
		VMemoryUsage::frame));

	vertex_pos = 0, index_pos = 0, num_elements = 0;
	auto &rgraph = device->renderGraph();
	// TODO: upload command with multiple regions
	for(auto &chunk : m_chunks) {
		EXPECT(rgraph << CmdUpload(chunk.positions, dc.vbuffer, vertex_pos));
		vertex_pos += chunk.positions.size() * sizeof(chunk.positions[0]);
		EXPECT(rgraph << CmdUpload(chunk.colors, dc.vbuffer, vertex_pos));
		vertex_pos += chunk.colors.size() * sizeof(chunk.colors[0]);
		EXPECT(rgraph << CmdUpload(chunk.tex_coords, dc.vbuffer, vertex_pos));
		vertex_pos += chunk.tex_coords.size() * sizeof(chunk.tex_coords[0]);
		EXPECT(rgraph << CmdUpload(chunk.indices, dc.ibuffer, index_pos));
		index_pos += chunk.indices.size() * sizeof(chunk.indices[0]);
		// TODO: store element matrices in single vector
		for(auto &element : chunk.elements) {
			EXPECT(rgraph << CmdUpload(cspan(&element.matrix, 1), dc.matrix_buffer,
									   num_elements * sizeof(Matrix4)));
			num_elements++;
		}
	}

	// TODO: scissors support

	// TODO: mo¿e instrukcja BindVertexBuffers/IndexBuffers z pointerem do pamiêci CPU?
	// automatycznie by by³y grupowane do buforów i uploadowane?

	DescriptorPoolSetup pool_setup;
	pool_setup.sizes[VDescriptorType::storage_buffer] = 1;
	pool_setup.sizes[VDescriptorType::combined_image_sampler] = num_elements;
	pool_setup.max_sets = 1 + num_elements;

	dc.descr_pool = EX_PASS(device->createDescriptorPool(pool_setup));
	dc.descr0 = EX_PASS(dc.descr_pool->alloc(pipes[0]->pipelineLayout(), 0));
	dc.descr0.update(
		{{.binding = 0, .type = VDescriptorType::storage_buffer, .data = dc.matrix_buffer}});

	// TODO: optimize this
	dc.tex_descrs.resize(num_elements);
	return dc;
}

Ex<void> Renderer2D::render(DrawCall &dc, VDeviceRef device, const VulkanPipelines &ctx) {
	auto &pipes = ctx.pipelines;

	// TODO: render_pass shouldn't be set up here, but outside
	// we just have to make sure that pipeline is compatible with it
	//
	// TODO: passing weak pointers to commands

	auto &rgraph = device->renderGraph();
	rgraph << CmdBindDescriptorSet{.index = 0, .set = &dc.descr0};

	uint vertex_pos = 0, index_pos = 0, num_elements = 0;
	for(auto &chunk : m_chunks) {
		uint sizes[3] = {uint(chunk.positions.size() * sizeof(chunk.positions[0])),
						 uint(chunk.colors.size() * sizeof(chunk.colors[0])),
						 uint(chunk.tex_coords.size() * sizeof(chunk.tex_coords[0]))};
		uint offsets[3];
		for(int i = 0; i < 3; i++)
			offsets[i] = vertex_pos, vertex_pos += sizes[i];

		rgraph << CmdBindVertexBuffers({dc.vbuffer, dc.vbuffer, dc.vbuffer}, cspan(offsets));
		rgraph << CmdBindIndexBuffer(dc.ibuffer, index_pos);
		index_pos += chunk.indices.size() * sizeof(chunk.indices[0]);

		for(auto &element : chunk.elements) {
			dc.tex_descrs[num_elements] =
				EX_PASS(dc.descr_pool->alloc(pipes[0]->pipelineLayout(), 1));
			auto tex = element.texture ? element.texture : ctx.white;
			dc.tex_descrs[num_elements].update({{.binding = 0,
												 .type = VDescriptorType::combined_image_sampler,
												 .data = pair{ctx.sampler, tex}}});
			// TODO: put update into command?
			rgraph << CmdBindDescriptorSet{.index = 1, .set = &dc.tex_descrs[num_elements]};

			int pipe_id = (element.blending_mode == BlendingMode::additive ? 1 : 0) +
						  (element.primitive_type == PrimitiveType::lines ? 2 : 0);
			rgraph << CmdBindPipeline{pipes[pipe_id]};
			rgraph << CmdDrawIndexed{.first_index = element.first_index,
									 .num_indices = element.num_indices,
									 .num_instances = 1,
									 .first_instance = int(num_elements)};
			num_elements++;
		}
	}

	return {};
}

vector<Pair<FRect, Matrix4>> Renderer2D::renderRects() const {
	vector<Pair<FRect, Matrix4>> out;
	for(const auto &chunk : m_chunks) {
		for(const auto &elem : chunk.elements) {
			const int *inds = &chunk.indices[elem.first_index];
			int min_index = inds[0], max_index = inds[0];
			for(int i = 0; i < elem.num_indices; i++) {
				max_index = max(max_index, inds[i]);
				min_index = min(min_index, inds[i]);
			}

			FRect erect = enclose(
				CSpan<float2>(&chunk.positions[min_index], &chunk.positions[max_index] + 1));
			out.emplace_back(erect, elem.matrix);
		}
	}

	return out;
}
}
