// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/canvas_3d.h"

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

// TODO: lot's of options for optimization, but we don't have time for that...

namespace fwk {

Canvas3D::Canvas3D(const IRect &viewport, const Matrix4 &proj_matrix, const Matrix4 &view_matrix)
	: m_matrix_stack(proj_matrix, view_matrix), m_viewport(viewport) {
	m_groups.emplace_back(0, getPipeline({}));
	m_group_matrices.emplace_back(m_matrix_stack.fullMatrix());
}

FWK_COPYABLE_CLASS_IMPL(Canvas3D);

Ex<SimpleDrawCall> Canvas3D::genDrawCall(ShaderCompiler &compiler, VulkanDevice &device,
										 PVRenderPass render_pass, VMemoryUsage mem_usage) {
	PERF_SCOPE();

	DASSERT_EQ(m_colors.size(), m_positions.size());
	DASSERT_EQ(m_tex_coords.size(), m_positions.size());

	SimpleDrawCall dc;

	auto vb_usage = VBufferUsage::vertex_buffer | VBufferUsage::transfer_dst;
	auto ib_usage = VBufferUsage::index_buffer | VBufferUsage::transfer_dst;
	auto mb_usage = VBufferUsage::storage_buffer | VBufferUsage::transfer_dst;

	uint num_verts = m_positions.size();
	uint vbuffer_size =
		num_verts * (sizeof(m_positions[0]) + sizeof(m_tex_coords[0]) + sizeof(m_colors[0]));
	auto vbuffer = EX_PASS(VulkanBuffer::create(device, vbuffer_size, vb_usage, mem_usage));

	dc.vertices = span<float3>(vbuffer, 0, num_verts);
	dc.tex_coords = span<TexCoord>(vbuffer, dc.vertices.byteEndOffset(), num_verts);
	dc.colors = span<IColor>(vbuffer, dc.tex_coords.byteEndOffset(), num_verts);

	EXPECT(dc.vertices.upload(m_positions));
	EXPECT(dc.tex_coords.upload(m_tex_coords));
	EXPECT(dc.colors.upload(m_colors));

	dc.indices = EX_PASS(VulkanBuffer::createAndUpload(device, m_indices, ib_usage, mem_usage));
	dc.instance_matrices =
		EX_PASS(VulkanBuffer::createAndUpload(device, m_group_matrices, mb_usage, mem_usage));

	bool skip_first_pipeline = true;
	for(auto &group : m_groups)
		if(group.pipeline_index == 0) {
			skip_first_pipeline = false;
			break;
		}

	dc.instances.reserve(m_groups.size());
	for(auto &group : m_groups) {
		if(group.num_indices == 0)
			continue;

		auto &instance = dc.instances.emplace_back();
		instance.texture = group.texture;
		instance.pipeline_index = group.pipeline_index + (skip_first_pipeline ? -1 : 0);
		instance.num_vertices = group.num_indices;
		instance.first_index = group.first_index;
	}

	auto setups = subSpan(m_pipelines, skip_first_pipeline ? 1 : 0);
	dc.pipelines = EX_PASS(SimpleDrawCall::makePipelines(compiler, device, render_pass, setups));
	return dc;
}

vector<Pair<FBox, Matrix4>> Canvas3D::drawBoxes() const {
	vector<Pair<FBox, Matrix4>> out;
	for(int g : intRange(m_groups)) {
		auto &group = m_groups[g];
		const u32 *inds = &m_indices[group.first_index];
		u32 min_index = inds[0], max_index = inds[0];
		for(int i = 0; i < group.num_indices; i++) {
			max_index = max(max_index, inds[i]);
			min_index = min(min_index, inds[i]);
		}

		auto ebox = enclose(CSpan<float3>(&m_positions[min_index], &m_positions[max_index] + 1));
		out.emplace_back(ebox, m_group_matrices[g]);
	}

	return out;
}

// --------------------------------------------------------------------------------------------
// ---------- Changing canvas state -----------------------------------------------------------

int Canvas3D::getPipeline(const SimplePipelineSetup &setup) {
	// TODO: add hashmap if necessary
	for(int i : intRange(m_pipelines))
		if(m_pipelines[i] == setup)
			return i;
	int index = m_pipelines.size();
	m_pipelines.emplace_back(setup);
	return index;
}

bool Canvas3D::splitGroup() {
	if(auto &group = m_groups.back(); group.num_indices > 0) {
		auto new_group = group;
		new_group.first_index = m_indices.size();
		new_group.num_indices = 0;
		m_groups.emplace_back(new_group);
		m_group_matrices.emplace_back(m_matrix_stack.fullMatrix());
		return true;
	}
	return false;
}

void Canvas3D::setPointWidth(float width) { m_point_width = width; }
void Canvas3D::setSegmentWidth(float width) { m_segment_width = width; }

void Canvas3D::setMaterial(const SimpleMaterial &material) {
	auto *group = &m_groups.back();
	auto &pipe = m_pipelines[group->pipeline_index];

	SimplePipelineSetup new_setup{material.drawingFlags(), material.blendingMode(),
								  VPrimitiveTopology::triangle_list};
	m_cur_color = FColor(material.color());
	if(material.texture() == group->texture && new_setup == pipe)
		return;

	splitGroup();
	group = &m_groups.back();
	group->pipeline_index = getPipeline(new_setup);
	group->texture = material.texture();
}

SimpleMaterial Canvas3D::getMaterial() const {
	auto &group = m_groups.back();
	auto &pipe = m_pipelines[group.pipeline_index];
	return SimpleMaterial{group.texture, IColor(m_cur_color), pipe.blending_mode, pipe.flags};
}

void Canvas3D::pushViewMatrix() {
	m_matrix_stack.pushViewMatrix();
	if(!splitGroup())
		m_group_matrices.back() = m_matrix_stack.fullMatrix();
}

void Canvas3D::popViewMatrix() {
	m_matrix_stack.popViewMatrix();
	if(!splitGroup())
		m_group_matrices.back() = m_matrix_stack.fullMatrix();
}

void Canvas3D::mulViewMatrix(const Matrix4 &matrix) {
	m_matrix_stack.mulViewMatrix(matrix);
	if(!splitGroup())
		m_group_matrices.back() = m_matrix_stack.fullMatrix();
}

void Canvas3D::setViewMatrix(const Matrix4 &matrix) {
	m_matrix_stack.setViewMatrix(matrix);
	if(!splitGroup())
		m_group_matrices.back() = m_matrix_stack.fullMatrix();
}

// --------------------------------------------------------------------------------------------
// ------------ Drawing functions -------------------------------------------------------------

// TODO: add option to pass only a single color, or single color per quad/triangle
// It should all be doable
void Canvas3D::appendColors(CSpan<IColor> colors, int num_vertices, int multiplier) {
	int old_size = m_colors.size();
	m_colors.resize(old_size + num_vertices);
	if(colors) {
		DASSERT_EQ(colors.size() * multiplier, num_vertices);
		int offset = old_size;
		if(m_cur_color == FColor(float4(1.0))) {
			for(int i : intRange(colors))
				for(int j : intRange(multiplier))
					m_colors[offset++] = colors[i];
		} else {
			for(int i : intRange(colors))
				for(int j : intRange(multiplier))
					m_colors[offset++] = IColor(FColor(colors[i]) * m_cur_color);
		}
	} else {
		fill(span(m_colors).subSpan(old_size), IColor(m_cur_color));
	}
}

void Canvas3D::appendQuadIndices(int vertex_offset, int num_quads) {
	int index_offset = m_indices.size();
	int num_indices = num_quads * 6;
	m_indices.resize(index_offset + num_indices);
	for(int i = 0; i < num_quads; i++) {
		uint inds[6] = {0, 1, 2, 0, 2, 3};
		for(int j = 0; j < 6; j++)
			m_indices[index_offset + j] = vertex_offset + inds[j];
		index_offset += 6;
		vertex_offset += 4;
	}
	m_groups.back().num_indices += num_indices;
}

void Canvas3D::appendQuadTexCoords(CSpan<TexCoord> tex_coords, int num_vertices) {
	int num_quads = num_vertices / 4;
	int old_size = m_tex_coords.size(), new_size = old_size + num_vertices;
	m_tex_coords.resize(new_size);
	if(tex_coords) {
		DASSERT_EQ(tex_coords.size(), num_vertices);
		copy(m_tex_coords.data() + old_size, tex_coords);
	} else {
		auto tex_coords = FRect(0, 0, 1, 1).corners();
		for(int i : intRange(num_quads))
			copy(m_tex_coords.data() + old_size + i * 4, tex_coords);
	}
}

void Canvas3D::addTris(CSpan<float3> points, CSpan<float2> tex_coords, CSpan<IColor> colors) {
	DASSERT(points.size() % 3 == 0);

	int num_tris = points.size() / 3;
	int old_size = m_positions.size(), new_size = old_size + points.size();
	m_positions.resize(new_size);
	copy(m_positions.data() + old_size, points);

	m_tex_coords.resize(new_size);
	if(tex_coords) {
		DASSERT_EQ(tex_coords.size(), num_tris * 3);
		copy(m_tex_coords.data() + old_size, tex_coords);
	} else {
		fill(span(m_tex_coords).subSpan(old_size), TexCoord());
	}

	appendColors(colors, points.size(), 1);
	int index_offset = m_indices.size();
	int vertex_offset = old_size;
	int num_indices = num_tris * 3;
	m_indices.resize(index_offset + num_indices);
	for(int i = 0; i < num_tris; i++) {
		uint inds[6] = {0, 1, 2};
		for(int j = 0; j < 3; j++)
			m_indices[index_offset + j] = inds[j] + vertex_offset;
		index_offset += 3;
		vertex_offset += 3;
	}
	m_groups.back().num_indices += num_indices;
}

void Canvas3D::addQuads(CSpan<float3> points, CSpan<TexCoord> tex_coords, CSpan<IColor> colors) {
	DASSERT_EQ(points.size() % 4, 0);

	int old_size = m_positions.size(), new_size = old_size + points.size();
	int num_quads = points.size() / 4;

	m_positions.resize(new_size);
	for(int i : intRange(points))
		m_positions[old_size + i] = points[i];

	appendQuadTexCoords(tex_coords, points.size());
	appendColors(colors, points.size(), 1);
	appendQuadIndices(old_size, num_quads);
}

void Canvas3D::addPoints(CSpan<float3> points, CSpan<IColor> colors) {
	auto corners = FRect(float2(-m_point_width) * 0.5f, float2(m_point_width) * 0.5f).corners();
	auto tex_corners = FRect(0, 0, 1, 1).corners();

	int num_vertices = points.size() * 4;
	int old_size = m_positions.size();
	m_positions.resize(old_size + num_vertices);
	for(int i : intRange(points))
		for(int j : intRange(corners))
			m_positions[old_size + i * 4 + j] = float3(corners[j], 0.0f) + points[i];

	m_tex_coords.resize(old_size + num_vertices);
	for(int i : intRange(points))
		copy(m_tex_coords.data() + i * 4, tex_corners);

	appendColors(colors, num_vertices, 4);
	appendQuadIndices(old_size, points.size());
}

void Canvas3D::addSegments(CSpan<float3> points, CSpan<IColor> colors) {
	DASSERT_EQ(points.size() % 2, 0);
	int old_size = m_positions.size();
	int num_segs = points.size() / 2;

	m_positions.resize(old_size + num_segs * 4);
	m_tex_coords.resize(old_size + num_segs * 4);

	// TODO: handle 3D case
	// TODO: how to handle degenerate segments?

	float half_width = m_segment_width * 0.5f;
	for(int i : intRange(num_segs)) {
		float3 p1 = points[i * 2 + 0], p2 = points[i * 2 + 1];
		auto dir = (p2 == p1 ? float3(1, 0, 0) : normalize(p2 - p1)) * half_width;
		auto perp = float3(perpendicular(dir.xy()), 0.0);

		m_positions[old_size + i * 4 + 0] = p1 - dir - perp;
		m_positions[old_size + i * 4 + 1] = p1 - dir + perp;
		m_positions[old_size + i * 4 + 2] = p2 + dir + perp;
		m_positions[old_size + i * 4 + 3] = p2 + dir - perp;
	}

	auto tex_coords = FRect(0, 0, 1, 1).corners();
	for(int i : intRange(num_segs))
		copy(m_tex_coords.data() + old_size + i * 4, tex_coords);

	appendColors(colors, points.size() * 2, 2);
	appendQuadIndices(old_size, num_segs);
}

void Canvas3D::addSegment(const float3 &p1, const float3 &p2, FColor color) {
	IColor icolor(color);
	addSegments({p1, p2}, {icolor, icolor});
}
}
