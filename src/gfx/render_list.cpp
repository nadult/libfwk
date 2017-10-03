// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/dtexture.h"
#include "fwk/gfx/gfx_device.h"
#include "fwk/gfx/vertex_array.h"
#include "fwk/gfx/vertex_buffer.h"
#include "fwk/gfx/program.h"
#include "fwk/gfx/program_binder.h"
#include "fwk/gfx/render_list.h"
#include "fwk/gfx/shader.h"
#include "fwk_opengl.h"
#include <algorithm>
#include <functional>

namespace fwk {

static const char *fsh_simple_src =
	"varying lowp vec4 color;\n"
	"void main() {\n"
	"  gl_FragColor = color;\n"
	"}\n";

static const char *fsh_tex_src =
	"uniform sampler2D tex; \n"
	"varying lowp vec4 color;\n"
	"varying mediump vec2 tex_coord;\n"
	"void main() {\n"
	"  gl_FragColor = color * texture2D(tex, tex_coord);\n"
	"}\n";

static const char *fsh_flat_src =
	"#extension GL_OES_standard_derivatives : enable\n"
	"\n"
	"varying lowp vec4 color;\n"
	"varying mediump vec3 tpos;\n"
	"\n"
	"void main() {\n"
	"   #ifdef SHADE\n"
	"     mediump vec3 normal = normalize(cross(dFdx(tpos), dFdy(tpos)));\n"
	"     mediump float shade = abs(dot(normal, vec3(0, 0, 1))) * 0.5 + 0.5;\n"
	"     gl_FragColor = color * shade;\n"
	"   #else\n"
	"     gl_FragColor = color;\n"
	"   #endif\n"
	"}\n";

static const char *vsh_src =
	"uniform mat4 proj_view_matrix;\n"
	"uniform vec4 mesh_color;\n"
	"attribute vec3 in_pos;\n"
	"attribute vec4 in_color;\n"
	"attribute vec2 in_tex_coord;\n"
	"varying vec2 tex_coord;\n"
	"varying vec4 color;\n"
	" varying vec3 tpos;\n"
	"void main() {\n"
	"  gl_Position = proj_view_matrix * vec4(in_pos, 1.0);\n"
	"  tpos = gl_Position.xyz;\n"
	"  tex_coord = in_tex_coord;\n"
	"  color = in_color * mesh_color;\n"
	"}\n";

struct ProgramFactory {
	PProgram operator()(const string &name) const {
		const char *src = name == "tex" ? fsh_tex_src : fsh_simple_src;
		if(name.find("flat") != string::npos)
			src = fsh_flat_src;
		bool shade = name.find("shade") != string::npos;

		string macros = shade ? "#version 100\n#define SHADE\n" : "#version 100\n";

		Shader vertex_shader(ShaderType::vertex, vsh_src, macros, name);
		Shader fragment_shader(ShaderType::fragment, src, macros, name);
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

void RenderList::add(CSpan<DrawCall> draws) {
	for(auto draw : draws)
		add(draw);
}

void RenderList::add(CSpan<DrawCall> draws, const Matrix4 &mat) {
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

	using MatFlags = MaterialFlags;
	using MatOpt = MaterialOpt;

	struct DeviceConfig {
		DeviceConfig() : flags(MatFlags::all()) { update(none); }

		void update(MatFlags new_flags) {
			if((new_flags & MatOpt::blended) != (flags & MatOpt::blended)) {
				if(new_flags & MatOpt::blended)
					glEnable(GL_BLEND);
				else
					glDisable(GL_BLEND);
			}
			if((new_flags & MatOpt::two_sided) != (flags & MatOpt::two_sided)) {
				if(new_flags & MatOpt::two_sided)
					glDisable(GL_CULL_FACE);
				else
					glEnable(GL_CULL_FACE);
			}
			if((new_flags & MatOpt::clear_depth) && !(flags & MatOpt::clear_depth)) {
				GfxDevice::clearDepth(1.0f);
			}
			if((new_flags & MatOpt::ignore_depth) != (flags & MatOpt::ignore_depth)) {
				bool do_enable = !(new_flags & MatOpt::ignore_depth);
				glDepthMask(do_enable);
				enable(GL_DEPTH_TEST, do_enable);
			}

			flags = new_flags;
		}

		MatFlags flags;
	};
}

void RenderList::render() {
	glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(1);

	DeviceConfig dev_config;
	auto tex_program = s_mgr["tex"];
	auto flat_program = s_mgr["flat"];
	auto flat_shade_program = s_mgr["flat_shade"];

	for(const auto &draw_call : m_draw_calls) {
		auto &mat = draw_call.material;
		if(!mat.textures.empty())
			DTexture::bind(mat.textures);
		PProgram program = !mat.textures.empty() ? tex_program : flat_shade_program;
		if(draw_call.primitiveType() == PrimitiveType::lines)
			program = flat_program;

		ProgramBinder binder(program);
		binder.bind();
		binder.setUniform("proj_view_matrix", projectionMatrix() * draw_call.matrix);
		binder.setUniform("mesh_color", (float4)FColor(mat.color));
		dev_config.update(mat.flags);

		draw_call.issue();
	}

	dev_config.update(MatOpt::blended | MatOpt::two_sided);
	glDepthMask(0);
	renderSprites();

	glDepthMask(1);
	renderLines();

	clear();
	DTexture::unbind();
}

vector<pair<FBox, Matrix4>> RenderList::renderBoxes() const {
	vector<pair<FBox, Matrix4>> out;
	out.reserve(m_draw_calls.size() + m_lines.instances().size() + m_sprites.instances().size());

	for(auto &dc : m_draw_calls)
		if(dc.bbox)
			out.emplace_back(*dc.bbox, dc.matrix);

	for(auto &inst : m_lines.instances())
		out.emplace_back(enclose(inst.positions), inst.matrix);
	for(auto &inst : m_sprites.instances())
		out.emplace_back(enclose(inst.positions), inst.matrix);

	return out;
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
					   : VertexArraySource(FColor(ColorId::white));
		VertexArray sprite_array({pos, col, tex});

		const auto &mat = sprite.material;
		DTexture::bind(mat.textures);
		PProgram program = mat.texture() ? tex_program : flat_program;

		ProgramBinder binder(program);
		binder.bind();
		if(mat.texture())
			binder.setUniform("tex", 0);
		binder.setUniform("proj_view_matrix", projectionMatrix() * sprite.matrix);
		binder.setUniform("mesh_color", (float4)mat.color);
		sprite_array.draw(PrimitiveType::triangles, sprite_array.size(), 0);
	}
}

void RenderList::renderLines() {
	DTexture::unbind();
	ProgramBinder binder(s_mgr["simple"]);
	binder.bind();

	for(const auto &inst : m_lines.instances()) {
		auto pos = make_immutable<VertexBuffer>(inst.positions);
		auto col = inst.colors.empty()
					   ? VertexArraySource(FColor(ColorId::white))
					   : VertexArraySource(make_immutable<VertexBuffer>(inst.colors));
		VertexArray line_array({pos, col, VertexArraySource(float2(0, 0))});

		binder.setUniform("mesh_color", (float4)inst.material_color);
		enable(GL_DEPTH_TEST, !(inst.material_flags & MatOpt::ignore_depth));
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
