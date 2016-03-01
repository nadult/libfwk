/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <algorithm>
#include <functional>

namespace fwk {

static const char *fragment_shader_simple_src =
	"#version 100\n"
	"varying lowp vec4 color;							\n"
	"void main() {										\n"
	"  gl_FragColor = color;							\n"
	"}													\n";

static const char *fragment_shader_tex_src =
	"#version 100\n"
	"uniform sampler2D tex; 											\n"
	"varying lowp vec4 color;											\n"
	"varying mediump vec2 tex_coord;									\n"
	"void main() {														\n"
	"  gl_FragColor = color * texture2D(tex, tex_coord);				\n"
	"}																	\n";

static const char *fragment_shader_flat_src =
	"#version 100															\n"
	"#extension GL_OES_standard_derivatives : enable						\n"
	"																		\n"
	"varying lowp vec4 color;												\n"
	"varying mediump vec3 tpos;												\n"
	"																		\n"
	"void main() {															\n"
	"	mediump vec3 normal = normalize(cross(dFdx(tpos), dFdy(tpos)));		\n"
	"	mediump float shade = abs(dot(normal, vec3(0, 0, 1))) * 0.5 + 0.5;	\n"
	"	gl_FragColor = color * shade;										\n"
	"}																		\n";

static const char *vertex_shader_src =
	"#version 100\n"
	"uniform mat4 proj_view_matrix;										\n"
	"uniform vec4 mesh_color;											\n"
	"attribute vec3 in_pos;												\n"
	"attribute vec4 in_color;											\n"
	"attribute vec2 in_tex_coord;										\n"
	"varying vec2 tex_coord;  											\n"
	"varying vec4 color;  												\n"
	" varying vec3 tpos;												\n"
	"void main() {														\n"
	"  gl_Position = proj_view_matrix * vec4(in_pos, 1.0);				\n"
	"  tpos = gl_Position.xyz;											\n"
	"  tex_coord = in_tex_coord;										\n"
	"  color = in_color * mesh_color;									\n"
	"} 																	\n";

struct ProgramFactory {
	PProgram operator()(const string &name) const {
		const char *src =
			name == "tex" ? fragment_shader_tex_src : name == "flat" ? fragment_shader_flat_src
																	 : fragment_shader_simple_src;

		Shader vertex_shader(ShaderType::vertex, vertex_shader_src, "", name);
		Shader fragment_shader(ShaderType::fragment, src, "", name);
		return make_immutable<Program>(vertex_shader, fragment_shader,
									   vector<string>{"in_pos", "in_color", "in_tex_coord"});
	}
};

Renderer::Renderer(const IRect &viewport, const Matrix4 &projection_matrix)
	: MatrixStack(projection_matrix), m_viewport(viewport) {
	static ResourceManager<Program, ProgramFactory> mgr;
	m_tex_program = mgr["tex"];
	m_flat_program = mgr["flat"];
	m_simple_program = mgr["simple"];
}

Renderer::~Renderer() = default;

void Renderer::addDrawCall(const DrawCall &draw_call, PMaterial material, const Matrix4 &matrix) {
	DASSERT(material);
	m_instances.emplace_back(Instance{viewMatrix() * matrix, std::move(material), draw_call});
}

Renderer::SpriteInstance &Renderer::spriteInstance(PMaterial mat, Matrix4 matrix, bool has_colors,
												   bool has_tex_coords) {
	SpriteInstance *inst = m_sprites.empty() ? nullptr : &m_sprites.back();
	matrix = viewMatrix() * matrix;

	if(!inst || has_tex_coords != !inst->tex_coords.empty() ||
	   has_colors != !inst->colors.empty() || mat != inst->material || matrix != inst->matrix) {
		m_sprites.emplace_back(SpriteInstance());
		inst = &m_sprites.back();
		inst->matrix = matrix;
		inst->material = mat;
	}

	return *inst;
}

Renderer::LineInstance &Renderer::lineInstance(Color col, uint flags, Matrix4 matrix,
											   bool has_colors) {
	LineInstance *inst = m_lines.empty() ? nullptr : &m_lines.back();
	matrix = viewMatrix() * matrix;

	if(!inst || has_colors != !inst->colors.empty() || col != inst->material_color ||
	   flags != inst->material_flags || matrix != inst->matrix) {
		m_lines.emplace_back(LineInstance());
		inst = &m_lines.back();
		inst->matrix = matrix;
		inst->material_color = col;
		inst->material_flags = flags;
	}

	return *inst;
}

void Renderer::addLines(CRange<float3> verts, CRange<Color> colors, PMaterial mat,
						const Matrix4 &matrix) {
	DASSERT(verts.size() % 2 == 0);
	DASSERT(colors.size() == verts.size() || colors.empty());
	DASSERT(mat);

	auto &inst = lineInstance(mat->color(), mat->flags(), matrix, true);
	insertBack(inst.positions, verts);
	insertBack(inst.colors, colors);
}

void Renderer::addLines(CRange<float3> verts, PMaterial material, const Matrix4 &matrix) {
	DASSERT(verts.size() % 2 == 0);
	DASSERT(material);

	auto &inst = lineInstance(material->color(), material->flags(), matrix, false);
	insertBack(inst.positions, verts);
}

void Renderer::addLines(CRange<float3> verts, Color color, const Matrix4 &matrix) {
	DASSERT(verts.size() % 2 == 0);
	auto &inst = lineInstance(color, 0u, matrix, false);
	insertBack(inst.positions, verts);
}

void Renderer::addSegments(CRange<Segment> segs, PMaterial material, const Matrix4 &matrix) {
	vector<float3> verts;
	for(const auto &seg : segs)
		insertBack(verts, {seg.origin(), seg.end()});
	addLines(verts, material, matrix);
}

void Renderer::addWireBox(const FBox &bbox, Color color, const Matrix4 &matrix) {
	float3 verts[8];
	bbox.getCorners(verts);

	int indices[] = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 3, 7, 2, 6};
	float3 out_verts[arraySize(indices)];
	for(int i = 0; i < arraySize(indices); i++)
		out_verts[i] = verts[indices[i]];
	addLines(out_verts, color, matrix);
}

void Renderer::addSprites(CRange<float3> verts, CRange<float2> tex_coords, CRange<Color> colors,
						  PMaterial material, const Matrix4 &matrix) {
	DASSERT(tex_coords.empty() || tex_coords.size() == verts.size());
	DASSERT(colors.empty() || colors.size() == verts.size());
	auto &inst = spriteInstance(material, matrix, !colors.empty(), !tex_coords.empty());
	insertBack(inst.positions, verts);
	insertBack(inst.tex_coords, tex_coords);
	insertBack(inst.colors, colors);
}
namespace {

	void enable(uint what, bool enable) {
		if(enable)
			::glEnable(what);
		else
			::glDisable(what);
	}

	struct DeviceConfig {
		DeviceConfig() : flags(~0u) { update(0); }

		void update(uint new_flags) {
			if((new_flags & Material::flag_blended) != (flags & Material::flag_blended)) {
				if(new_flags & Material::flag_blended)
					glEnable(GL_BLEND);
				else
					glDisable(GL_BLEND);
			}
			if((new_flags & Material::flag_two_sided) != (flags & Material::flag_two_sided)) {
				if(new_flags & Material::flag_two_sided)
					glDisable(GL_CULL_FACE);
				else
					glEnable(GL_CULL_FACE);
			}
			if((new_flags & Material::flag_clear_depth) && !(flags & Material::flag_clear_depth)) {
				GfxDevice::clearDepth(1.0f);
			}
			if((new_flags & Material::flag_ignore_depth) != (flags & Material::flag_ignore_depth)) {
				bool do_enable = !(new_flags & Material::flag_ignore_depth);
				glDepthMask(do_enable);
				enable(GL_DEPTH_TEST, do_enable);
			}

			flags = new_flags;
		}

		uint flags;
	};
}

void Renderer::render() {
	glViewport(m_viewport.min.x, m_viewport.min.y, m_viewport.width(), m_viewport.height());
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(1);

	DeviceConfig dev_config;

	for(const auto &instance : m_instances) {
		auto material = instance.material;

		DTexture::bind(material->textures());
		PProgram program = material->texture() ? m_tex_program : m_flat_program;

		ProgramBinder binder(program);
		binder.bind();
		binder.setUniform("proj_view_matrix", projectionMatrix() * instance.matrix);
		binder.setUniform("mesh_color", (float4)material->color());
		dev_config.update(material->flags());

		instance.draw_call.issue();
	}

	dev_config.update(Material::flag_blended | Material::flag_two_sided);
	glDepthMask(0);
	renderSprites();

	glDepthMask(1);
	renderLines();

	clear();
	DTexture::unbind();
}

void Renderer::renderSprites() {
	// TODO: divide into regions, sort each region by Z
	for(const auto &sprite : m_sprites) {
		auto pos = make_immutable<VertexBuffer>(sprite.positions);
		auto tex = !sprite.tex_coords.empty()
					   ? VertexArraySource(make_immutable<VertexBuffer>(sprite.tex_coords))
					   : VertexArraySource(float2(0, 0));
		auto col = !sprite.colors.empty()
					   ? VertexArraySource(make_immutable<VertexBuffer>(sprite.colors))
					   : VertexArraySource(Color::white);
		VertexArray sprite_array({pos, col, tex});

		const auto &mat = sprite.material;
		DTexture::bind(mat->textures());
		PProgram program = mat->texture() ? m_tex_program : m_flat_program;

		ProgramBinder binder(program);
		binder.bind();
		if(mat->texture())
			binder.setUniform("tex", 0);
		binder.setUniform("proj_view_matrix", projectionMatrix() * sprite.matrix);
		binder.setUniform("mesh_color", (float4)mat->color());
		sprite_array.draw(PrimitiveType::triangles, sprite_array.size(), 0);
	}
}

void Renderer::renderLines() {
	DTexture::unbind();
	ProgramBinder binder(m_simple_program);
	for(const auto &inst : m_lines) {
		auto pos = make_immutable<VertexBuffer>(inst.positions);
		auto col = inst.colors.empty()
					   ? VertexArraySource(Color::white)
					   : VertexArraySource(make_immutable<VertexBuffer>(inst.colors));
		VertexArray line_array({pos, col, VertexArraySource(float2(0, 0))});

		binder.setUniform("mesh_color", (float4)inst.material_color);
		enable(GL_DEPTH_TEST, !(inst.material_flags & Material::flag_ignore_depth));
		binder.setUniform("proj_view_matrix", projectionMatrix() * inst.matrix);
		line_array.draw(PrimitiveType::lines, line_array.size(), 0);
	}
}

void Renderer::clear() {
	m_instances.clear();
	m_sprites.clear();
	m_lines.clear();
}
}
