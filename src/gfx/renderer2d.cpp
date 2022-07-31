// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/renderer2d.h"

#include "fwk/gfx/drawing.h"
#include "fwk/gfx/image.h"
#include "fwk/gfx/shader_compiler.h"
#include "fwk/hash_map.h"
#include "fwk/index_range.h"
#include "fwk/io/xml.h"
#include "fwk/math/constants.h"
#include "fwk/math/rotation.h"
#include "fwk/perf_base.h"
#include "fwk/sys/on_fail.h"
#include "fwk/vulkan/vulkan_buffer_span.h"
#include "fwk/vulkan/vulkan_command_queue.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_shader.h"

// Czy chcemy, ¿eby Renderer2D bazowa³ na ElementBufferze ?
// Czy jest sens to robiæ teraz ? nie, zróbmy tylko, ¿eby dzia³a³o, nawet bez optymalizacji?

namespace fwk {

Renderer2D::BasicMaterial::BasicMaterial(const SimpleMaterial &mat)
	: texture(mat.texture()), blending_mode(mat.blendingMode()) {}
bool Renderer2D::BasicMaterial::operator==(const BasicMaterial &rhs) const {
	return texture == rhs.texture && blending_mode == rhs.blending_mode;
}

Renderer2D::Renderer2D(const IRect &viewport, Orient2D orient)
	: MatrixStack(projectionMatrix2D(viewport, orient), viewMatrix2D(viewport, float2(0, 0))),
	  m_viewport(viewport) {}

Renderer2D::~Renderer2D() = default;

void Renderer2D::setViewPos(const float2 &view_pos) {
	MatrixStack::setViewMatrix(viewMatrix2D(m_viewport, view_pos));
}

Renderer2D::Element &Renderer2D::makeElement(VPrimitiveTopology topo,
											 const SimpleMaterial &material) {
	// TODO: merging won't work for triangle strip (have to add some more indices)
	BasicMaterial basic_mat(material);
	if(!m_elements || m_elements.back().prim_topo != topo ||
	   m_elements.back().material != basic_mat || fullMatrix() != m_matrices.back()) {
		m_matrices.emplace_back(fullMatrix());
		m_elements.emplace_back(Element{basic_mat, m_indices.size(), 0, topo});
	}
	return m_elements.back();
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
	auto &elem = makeElement(VPrimitiveTopology::triangle_list, {});
	uint idx = m_positions.size();

	auto ang_mul = pi * 2.0f / float(num_tris);
	for(int i = 0; i < num_tris; i++)
		m_positions.emplace_back(float3(angleToVector(float(i) * ang_mul) * size + center, 0.0));
	m_positions.emplace_back(float3(center, 0.0));

	for(int i = 0; i < num_tris; i++) {
		uint next = i == num_tris - 1 ? 0 : i + 1;
		insertBack(m_indices, {idx + num_tris, idx + i, idx + next});
	}

	m_tex_coords.resize(m_positions.size(), float2());
	m_colors.resize(m_positions.size(), IColor(color));
	elem.num_indices += num_tris * 3;
}

void Renderer2D::addEllipse(float2 center, float2 size, FColor color, int num_edges) {
	auto &elem = makeElement(VPrimitiveTopology::line_list, {});

	auto ang_mul = pi * 2.0f / float(num_edges);
	uint idx = m_positions.size();
	for(int i = 0; i < num_edges; i++) {
		m_positions.emplace_back(float3(angleToVector(float(i) * ang_mul) * size + center, 0.0));
		uint next = i == num_edges - 1 ? 0 : i + 1;
		insertBack(m_indices, {idx + i, idx + next});
	}
	m_tex_coords.resize(m_positions.size(), float2());
	m_colors.resize(m_positions.size(), IColor(color));
	elem.num_indices += num_edges * 2;
}

void Renderer2D::addRect(const FRect &rect, FColor color) {
	Element &elem = makeElement(VPrimitiveTopology::line_list, {});
	uint vertex_offset = m_positions.size();
	appendVertices(rect.corners(), {}, {}, color);

	const int num_indices = 8;
	uint indices[num_indices] = {0, 1, 1, 2, 2, 3, 3, 0};
	for(int i = 0; i < num_indices; i++)
		m_indices.emplace_back(vertex_offset + indices[i]);
	elem.num_indices += num_indices;
}

void Renderer2D::addLine(const float2 &p1, const float2 &p2, FColor color) {
	Element &elem = makeElement(VPrimitiveTopology::line_list, {});
	uint vertex_offset = m_positions.size();
	appendVertices({p1, p2}, {}, {}, color);

	insertBack(m_indices, {vertex_offset, vertex_offset + 1});
	elem.num_indices += 2;
}

void Renderer2D::appendVertices(CSpan<float2> positions, CSpan<float2> tex_coords,
								CSpan<FColor> colors, FColor mat_color) {
	DASSERT(colors.size() == positions.size() || !colors);
	DASSERT(tex_coords.size() == positions.size() || !tex_coords);

	// TODO
	insertBack(m_positions, transform(positions, [](float2 pos) { return float3(pos, 0.0); }));
	if(colors)
		for(auto col : colors)
			m_colors.emplace_back(col * mat_color);
	else
		m_colors.resize(m_positions.size(), IColor(mat_color));
	if(tex_coords)
		insertBack(m_tex_coords, tex_coords);
	else
		m_tex_coords.resize(m_positions.size(), float2());
}

void Renderer2D::addLines(CSpan<float2> pos, CSpan<FColor> color, FColor mat_color) {
	DASSERT(pos.size() % 2 == 0);

	Element &elem = makeElement(VPrimitiveTopology::line_list, {});

	uint vertex_offset = m_positions.size();
	appendVertices(pos, {}, color, mat_color);
	for(int i = 0; i < pos.size(); i++)
		m_indices.emplace_back(vertex_offset + i);
	elem.num_indices += pos.size();
}

void Renderer2D::addQuads(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor> color,
						  const SimpleMaterial &material) {
	DASSERT(pos.size() % 4 == 0);

	Element &elem = makeElement(VPrimitiveTopology::triangle_list, material);
	uint vertex_offset = m_positions.size();
	int num_quads = pos.size() / 4;

	appendVertices(pos, tex_coord, color, material.color());
	for(int i = 0; i < num_quads; i++) {
		uint inds[6] = {0, 1, 2, 0, 2, 3};
		for(int j = 0; j < 6; j++)
			m_indices.emplace_back(vertex_offset + i * 4 + inds[j]);
	}
	elem.num_indices += num_quads * 6;
}

void Renderer2D::addTris(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor> color,
						 const SimpleMaterial &material) {
	DASSERT(pos.size() % 3 == 0);

	int num_tris = pos.size() / 3;
	Element &elem = makeElement(VPrimitiveTopology::triangle_list, material);
	uint vertex_offset = m_positions.size();
	appendVertices(pos, tex_coord, color, material.color());

	for(int i = 0; i < num_tris; i++) {
		uint inds[3] = {0, 1, 2};
		for(int j = 0; j < 3; j++)
			m_indices.emplace_back(vertex_offset + i * 3 + inds[j]);
	}
	elem.num_indices += num_tris * 3;
}

Ex<SimpleDrawCall> Renderer2D::genDrawCall(ShaderCompiler &compiler, VulkanDevice &device,
										   PVRenderPass render_pass, VMemoryUsage mem_usage) {
	PERF_SCOPE();

	SimpleDrawCall dc;

	auto vb_usage = VBufferUsage::vertex_buffer | VBufferUsage::transfer_dst;
	auto ib_usage = VBufferUsage::index_buffer | VBufferUsage::transfer_dst;
	auto mb_usage = VBufferUsage::storage_buffer | VBufferUsage::transfer_dst;

	dc.vertices = EX_PASS(VulkanBuffer::createAndUpload(device, m_positions, vb_usage, mem_usage));
	dc.tex_coords =
		EX_PASS(VulkanBuffer::createAndUpload(device, m_tex_coords, vb_usage, mem_usage));
	dc.colors = EX_PASS(VulkanBuffer::createAndUpload(device, m_colors, vb_usage, mem_usage));
	dc.indices = EX_PASS(VulkanBuffer::createAndUpload(device, m_indices, ib_usage, mem_usage));
	dc.instance_matrices =
		EX_PASS(VulkanBuffer::createAndUpload(device, m_matrices, mb_usage, mem_usage));

	vector<SimplePipelineSetup> setups;
	setups.reserve(count<SimpleBlendingMode> * count<VPrimitiveTopology>);

	dc.instances.reserve(m_elements.size());
	for(auto &element : m_elements) {
		auto &instance = dc.instances.emplace_back();
		instance.texture = element.material.texture;

		int pipeline_index = -1;
		for(int i : intRange(setups))
			if(setups[i].blending_mode == element.material.blending_mode &&
			   setups[i].primitive_topo == element.prim_topo) {
				pipeline_index = i;
				break;
			}

		if(pipeline_index == -1) {
			pipeline_index = setups.size();
			setups.emplace_back(none, element.material.blending_mode, element.prim_topo);
		}
		instance.pipeline_index = pipeline_index;
		instance.num_vertices = element.num_indices;
		instance.first_index = element.first_index;
	}

	dc.pipelines = EX_PASS(SimpleDrawCall::makePipelines(compiler, device, render_pass, setups));
	return dc;
}

vector<Pair<FRect, Matrix4>> Renderer2D::drawRects() const {
	vector<Pair<FRect, Matrix4>> out;
	for(int e : intRange(m_elements)) {
		auto &elem = m_elements[e];
		const u32 *inds = &m_indices[elem.first_index];
		u32 min_index = inds[0], max_index = inds[0];
		for(int i = 0; i < elem.num_indices; i++) {
			max_index = max(max_index, inds[i]);
			min_index = min(min_index, inds[i]);
		}

		auto ebox = enclose(CSpan<float3>(&m_positions[min_index], &m_positions[max_index] + 1));
		out.emplace_back(ebox.xy(), m_matrices[e]);
	}

	return out;
}
}
