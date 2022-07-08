// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/renderer2d.h"
#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx/gl_device.h"
#include "fwk/gfx/gl_program.h"
#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx/gl_texture.h"
#include "fwk/gfx/gl_vertex_array.h"
#include "fwk/gfx/opengl.h"
#include "fwk/hash_map.h"
#include "fwk/io/xml.h"
#include "fwk/math/constants.h"
#include "fwk/math/rotation.h"
#include "fwk/sys/on_fail.h"

#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_render_graph.h"
#include "fwk/vulkan/vulkan_shader.h"

namespace fwk {

static const char *fragment_shader_2d_tex_src =
	"#version 100\n"
	"uniform sampler2D tex; 											\n"
	"varying lowp vec4 color;											\n"
	"varying mediump vec2 tex_coord;									\n"
	"void main() {														\n"
	"	gl_FragColor = color * texture2D(tex, tex_coord);				\n"
	"}																	\n";

static const char *fragment_shader_2d_flat_src =
	"#version 100\n"
	"varying lowp vec4 color;											\n"
	"void main() {														\n"
	"	gl_FragColor = color;											\n"
	"}																	\n";

static const char *vertex_shader_2d_src =
	"#version 100\n"
	"uniform mat4 proj_view_matrix;										\n"
	"attribute vec2 in_pos;												\n"
	"attribute vec4 in_color;											\n"
	"attribute vec2 in_tex_coord;										\n"
	"varying vec2 tex_coord;  											\n"
	"varying vec4 color;  												\n"
	"void main() {														\n"
	"	gl_Position = proj_view_matrix * vec4(in_pos, 0.0, 1.0);		\n"
	"	tex_coord = in_tex_coord;										\n"
	"	color = in_color;												\n"
	"} 																	\n";

const char *vsh_2d_vulkan = R"(
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

const char *fsh_2d_vulkan = R"(
#version 450

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_tex_coord;

//layout(set = 1, binding = 0) uniform sampler2D tex_sampler;

void main() {
	out_color = in_color;// * texture(tex_sampler, in_tex_coord);
}
)";

static Ex<PProgram> buildProgram2D(Str name) {
	ON_FAIL("Building shader program: %", name);
	const char *src =
		name == "2d_with_texture" ? fragment_shader_2d_tex_src : fragment_shader_2d_flat_src;
	auto vsh = EX_PASS(GlShader::compileAndCheck(ShaderType::vertex, vertex_shader_2d_src));
	auto fsh = EX_PASS(GlShader::compileAndCheck(ShaderType::fragment, src));
	return GlProgram::linkAndCheck({vsh, fsh}, {"in_pos", "in_color", "in_tex_coord"});
}

static PProgram getProgram2D(Str name) {
	if(auto out = GlDevice::instance().cacheFindProgram(name))
		return out;
	auto program = buildProgram2D(name).get();
	GlDevice::instance().cacheAddProgram(name, program);
	return program;
}

Renderer2D::Renderer2D(const IRect &viewport, Orient2D orient)
	: MatrixStack(projectionMatrix2D(viewport, orient), viewMatrix2D(viewport, float2(0, 0))),
	  m_viewport(viewport), m_current_scissor_rect(-1) {
	if(GlDevice::isPresent()) {
		m_tex_program = getProgram2D("2d_with_texture");
		m_tex_program["tex"] = 0;
		m_flat_program = getProgram2D("2d_without_texture");
	}
}

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
											 PTexture texture, BlendingMode bm) {
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
	Element &elem = makeElement(chunk, PrimitiveType::lines, PTexture());
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
	Element &elem = makeElement(chunk, PrimitiveType::lines, PTexture());
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
	Element &elem = makeElement(chunk, PrimitiveType::lines, PTexture());

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

auto Renderer2D::makeVulkanPipelines(VDeviceRef device, VkFormat draw_surface_format)
	-> Ex<VulkanPipelines> {
	VRenderPassSetup rp_setup;
	rp_setup.colors = {{VAttachmentCore(draw_surface_format, 1)}};
	rp_setup.colors_sync.emplace_back(VColorAttachmentSync(
		VLoadOp::clear, VStoreOp::store, VLayout::undefined, VLayout::present_src));
	auto render_pass = EX_PASS(VulkanRenderPass::create(device, rp_setup));

	VPipelineSetup setup;
	setup.shader_modules = EX_PASS(VulkanShaderModule::compile(
		device, {{VShaderStage::vertex, vsh_2d_vulkan}, {VShaderStage::fragment, fsh_2d_vulkan}}));
	setup.render_pass = render_pass;
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
	return out;
}

Ex<void> Renderer2D::render(VDeviceRef device, const VulkanPipelines &pipes_) {
	auto &pipes = pipes_.pipelines;

	// Shadery uzywaj¹ 2 rodzajów deskryptorów:
	// - jeden sta³y zawiraj¹cy macierze
	// - drugi dynamiczny zawieraj¹cy teksturki
	//
	// Co klatkê dla ka¿dej teksturki muszê wygenerowaæ descriptor_set i podpi¹æ go ?
	// DLa ka¿dego typu teksturki, nie dla ka¿dego elementu

	// Mo¿e zrobiæ max 128 ró¿nych materia³ów, i trzymaæ je w tablicach?
	// Ale, renderer2d mo¿e wymagaæ

	// Po prostu jak bêdzie du¿o tekstur to bêdzie wolne?
	// Poza tym mogê zrobiæ HashMap<PVTexture, descriptor> ?

	// Mo¿e lepiej by by³o lepiej przerobiæ trochê

	uint vertex_pos = 0;
	uint index_pos = 0;
	uint num_elements = 0;

	for(auto &chunk : m_chunks) {
		vertex_pos += chunk.positions.size() * sizeof(chunk.positions[0]) +
					  chunk.colors.size() * sizeof(chunk.colors[0]) +
					  chunk.tex_coords.size() * sizeof(chunk.tex_coords[0]);
		index_pos += chunk.indices.size() * sizeof(chunk.indices[0]);
		num_elements += chunk.elements.size();
	}

	auto vbuffer = EX_PASS(VulkanBuffer::create(
		device, vertex_pos, VBufferUsage::vertex_buffer | VBufferUsage::transfer_dst,
		VMemoryUsage::frame));
	auto ibuffer = EX_PASS(VulkanBuffer::create(
		device, index_pos, VBufferUsage::index_buffer | VBufferUsage::transfer_dst,
		VMemoryUsage::frame));
	auto matrix_buffer = EX_PASS(VulkanBuffer::create<Matrix4>(
		device, num_elements, VBufferUsage::storage_buffer | VBufferUsage::transfer_dst,
		VMemoryUsage::frame));

	vertex_pos = 0, index_pos = 0, num_elements = 0;
	auto &rgraph = device->renderGraph();
	// TODO: upload command with multiple regions
	for(auto &chunk : m_chunks) {
		EXPECT(rgraph << CmdUpload(chunk.positions, vbuffer, vertex_pos));
		vertex_pos += chunk.positions.size() * sizeof(chunk.positions[0]);
		EXPECT(rgraph << CmdUpload(chunk.colors, vbuffer, vertex_pos));
		vertex_pos += chunk.colors.size() * sizeof(chunk.colors[0]);
		EXPECT(rgraph << CmdUpload(chunk.tex_coords, vbuffer, vertex_pos));
		vertex_pos += chunk.tex_coords.size() * sizeof(chunk.tex_coords[0]);
		EXPECT(rgraph << CmdUpload(chunk.indices, ibuffer, index_pos));
		index_pos += chunk.indices.size() * sizeof(chunk.indices[0]);
		// TODO: store element matrices in single vector
		for(auto &element : chunk.elements) {
			EXPECT(rgraph << CmdUpload(cspan(&element.matrix, 1), matrix_buffer,
									   num_elements * sizeof(Matrix4)));
			num_elements++;
		}
	}

	// TODO: mo¿e instrukcja BindVertexBuffers/IndexBuffers z pointerem do pamiêci CPU?
	// automatycznie by by³y grupowane do buforów i uploadowane?

	DescriptorPoolSetup pool_setup;
	pool_setup.sizes[VDescriptorType::storage_buffer] = 16;
	auto descriptor_pool = EX_PASS(device->createDescriptorPool(pool_setup));
	auto descr = EX_PASS(descriptor_pool->alloc(pipes[0]->pipelineLayout(), 0));
	descr.update({{.binding = 0, .type = VDescriptorType::storage_buffer, .data = matrix_buffer}});

	// TODO: render_pass shouldn't be set up here, but outside
	// we just have to make sure that pipeline is compatible with it
	//
	// TODO: passing weak pointers to commands
	rgraph << CmdBeginRenderPass{
		pipes[0]->renderPass(), none, {{VkClearColorValue{0.0, 0.2, 0.0, 1.0}}}};

	rgraph << CmdBindDescriptorSet{.index = 0, .set = &descr};
	vertex_pos = 0, index_pos = 0, num_elements = 0;
	for(auto &chunk : m_chunks) {
		uint sizes[3] = {uint(chunk.positions.size() * sizeof(chunk.positions[0])),
						 uint(chunk.colors.size() * sizeof(chunk.colors[0])),
						 uint(chunk.tex_coords.size() * sizeof(chunk.tex_coords[0]))};
		uint offsets[3];
		for(int i = 0; i < 3; i++)
			offsets[i] = vertex_pos, vertex_pos += sizes[i];

		rgraph << CmdBindVertexBuffers({vbuffer, vbuffer, vbuffer}, cspan(offsets));
		rgraph << CmdBindIndexBuffer(ibuffer, index_pos);
		index_pos += chunk.indices.size() * sizeof(chunk.indices[0]);

		for(auto &element : chunk.elements) {
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
	rgraph << CmdEndRenderPass{};

	// TODO: final layout has to be present, middle layout shoud be different FFS! How to automate this ?!?
	// Only queue uploads ?
	rgraph.flushCommands();

	return {};
}

void Renderer2D::render() {
	glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	glDisable(GL_SCISSOR_TEST);

	int prev_scissor_rect = -1;
	auto bm = BlendingMode::normal;

	for(const auto &chunk : m_chunks) {
		auto pbuffer = GlBuffer::make(BufferType::array, chunk.positions);
		auto cbuffer = GlBuffer::make(BufferType::array, chunk.colors);
		auto tbuffer = GlBuffer::make(BufferType::array, chunk.tex_coords);
		auto ibuffer = GlBuffer::make(BufferType::element_array, chunk.indices);

		auto vao = GlVertexArray::make();
		vao->set({move(pbuffer), move(cbuffer), move(tbuffer)},
				 defaultVertexAttribs<float2, IColor, float2>(), move(ibuffer), IndexType::uint32);

		for(const auto &element : chunk.elements) {
			auto &program = (element.texture ? m_tex_program : m_flat_program);
			program["proj_view_matrix"] = element.matrix;

			if(element.texture)
				element.texture->bind();
			if(bm != element.blending_mode) {
				bm = element.blending_mode;
				if(bm == BlendingMode::normal)
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				else if(bm == BlendingMode::additive)
					glBlendFunc(GL_ONE, GL_ONE);
			}

			if(element.scissor_rect_id != prev_scissor_rect) {
				if(element.scissor_rect_id == -1)
					glDisable(GL_SCISSOR_TEST);
				else if(prev_scissor_rect == -1)
					glEnable(GL_SCISSOR_TEST);

				if(element.scissor_rect_id != -1) {
					IRect rect = m_scissor_rects[element.scissor_rect_id];
					int min_y = m_viewport.height() - rect.ey();
					rect = {{rect.x(), max(0, min_y)}, {rect.ex(), min_y + rect.height()}};
					glScissor(rect.x(), rect.y(), rect.width(), rect.height());
				}

				prev_scissor_rect = element.scissor_rect_id;
			}

			program->use();
			vao->draw(element.primitive_type, element.num_indices, element.first_index);
		}
	}

	glDisable(GL_SCISSOR_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GlTexture::unbind();

	clear();
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
