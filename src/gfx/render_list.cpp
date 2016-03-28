/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <algorithm>
#include <functional>

namespace fwk {

DrawCall::DrawCall(PVertexArray vertex_array, PrimitiveType primitive_type, int vertex_count,
				   int index_offset, PMaterial material, Matrix4 matrix)
	: matrix(matrix), material(material), m_vertex_array(move(vertex_array)),
	  m_primitive_type(primitive_type), m_vertex_count(vertex_count), m_index_offset(index_offset) {
	DASSERT(m_vertex_array);
}

void DrawCall::issue() const {
	m_vertex_array->draw(m_primitive_type, m_vertex_count, m_index_offset);
}

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

static ResourceManager<Program, ProgramFactory> s_mgr;

RenderList::RenderList(const IRect &viewport, const Matrix4 &projection_matrix)
	: MatrixStack(projection_matrix), m_sprites(*this), m_lines(*this), m_viewport(viewport) {}

RenderList::~RenderList() = default;

void RenderList::add(DrawCall draw_call) {
	draw_call.matrix = viewMatrix() * draw_call.matrix;
	m_draw_calls.emplace_back(move(draw_call));
}

void RenderList::add(DrawCall draw_call, const Matrix4 &matrix) {
	draw_call.matrix = viewMatrix() * matrix * draw_call.matrix;
	m_draw_calls.emplace_back(move(draw_call));
}

void RenderList::add(CRange<DrawCall> draws) {
	for(auto draw : draws)
		add(draw);
}

void RenderList::add(CRange<DrawCall> draws, const Matrix4 &mat) {
	for(auto draw : draws)
		add(draw, mat);
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

void RenderList::render() {
	glViewport(m_viewport.min.x, m_viewport.min.y, m_viewport.width(), m_viewport.height());
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(1);

	DeviceConfig dev_config;
	auto tex_program = s_mgr["tex"];
	auto flat_program = s_mgr["flat"];

	for(const auto &draw_call : m_draw_calls) {
		auto mat = draw_call.material;
		if(mat)
			DTexture::bind(mat->textures());
		PProgram program = mat && mat->texture() ? tex_program : flat_program;

		ProgramBinder binder(program);
		binder.bind();
		binder.setUniform("proj_view_matrix", projectionMatrix() * draw_call.matrix);
		binder.setUniform("mesh_color", (float4)(mat ? mat->color() : Color::white));
		dev_config.update(mat ? mat->flags() : 0);

		draw_call.issue();
	}

	dev_config.update(Material::flag_blended | Material::flag_two_sided);
	glDepthMask(0);
	renderSprites();

	glDepthMask(1);
	renderLines();

	clear();
	DTexture::unbind();
}

void RenderList::renderSprites() {
	auto tex_program = s_mgr["tex"];
	auto flat_program = s_mgr["flat"];

	// TODO: divide into regions, sort each region by Z
	for(const auto &sprite : m_sprites.instances()) {
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
		PProgram program = mat->texture() ? tex_program : flat_program;

		ProgramBinder binder(program);
		binder.bind();
		if(mat->texture())
			binder.setUniform("tex", 0);
		binder.setUniform("proj_view_matrix", projectionMatrix() * sprite.matrix);
		binder.setUniform("mesh_color", (float4)mat->color());
		sprite_array.draw(PrimitiveType::triangles, sprite_array.size(), 0);
	}
}

void RenderList::renderLines() {
	DTexture::unbind();
	ProgramBinder binder(s_mgr["simple"]);
	for(const auto &inst : m_lines.instances()) {
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

void RenderList::clear() {
	m_draw_calls.clear();
	m_sprites.clear();
	m_lines.clear();
}
}
