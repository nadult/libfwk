// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/render_list.h"

#include "fwk/gfx/colored_triangle.h"
#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx/gl_device.h"
#include "fwk/gfx/gl_program.h"
#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx/gl_texture.h"
#include "fwk/gfx/gl_vertex_array.h"
#include "fwk/gfx/opengl.h"
#include "fwk/hash_map.h"

namespace fwk {

// clang-format off
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
	"varying vec3 tpos;\n"
	"void main() {\n"
	"  gl_Position = proj_view_matrix * vec4(in_pos, 1.0);\n"
	"  tpos = gl_Position.xyz;\n"
	"  tex_coord = in_tex_coord;\n"
	"  color = in_color * mesh_color;\n"
	"}\n";
// clang-format on

// TODO: return Ex<>
static PProgram getProgram(Str name) {
	if(auto out = GlDevice::instance().cacheFindProgram(name))
		return out;

	const char *fsh_src = name == "tex" ? fsh_tex_src : fsh_simple_src;
	if(name.contains("flat"))
		fsh_src = fsh_flat_src;
	bool shade = name.contains("shade");

	string macros = shade ? "#version 100\n#define SHADE\n" : "#version 100\n";

	auto vsh = GlShader::make(ShaderType::vertex, {macros, vsh_src}, name).get();
	auto fsh = GlShader::make(ShaderType::fragment, {macros, fsh_src}, name).get();
	auto out = GlProgram::make(vsh, fsh, {"in_pos", "in_color", "in_tex_coord"}).get();
	GlDevice::instance().cacheAddProgram(name, out);
	return out;
}

RenderList::RenderList(const IRect &viewport, const Matrix4 &projection_matrix)
	: MatrixStack(projection_matrix), m_viewport(viewport) {}

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
		DeviceConfig() : flags(all<MatOpt>) { update(none); }

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
				clearDepth(1.0f);
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

void RenderList::render(bool mode_2d) {
	glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(1);

	DeviceConfig dev_config;
	auto tex_program = getProgram("tex");
	auto flat_program = mode_2d ? getProgram("flat") : getProgram("flat_shade");
	auto simple_program = getProgram("simple");

	for(const auto &draw_call : m_draw_calls) {
		auto &mat = draw_call.material;
		if(mat.textures)
			GlTexture::bind(mat.textures);

		PProgram program = mat.textures ? tex_program : flat_program;
		if(draw_call.primitive_type == PrimitiveType::lines)
			program = simple_program;

		program["proj_view_matrix"] = projectionMatrix() * draw_call.matrix;
		program["mesh_color"] = (float4)FColor(mat.color);

		auto flags =
			mat.flags | mask(mode_2d, MatOpt::blended | MatOpt::ignore_depth | MatOpt::two_sided);
		dev_config.update(flags);

		program->use();
		draw_call.issue();
	}

	/*
	dev_config.update(MatOpt::blended | MatOpt::two_sided);
	glDepthMask(0);
	renderSprites();
	*/

	clear();
	GlTexture::unbind();
}

vector<Pair<FBox, Matrix4>> RenderList::renderBoxes() const {
	vector<Pair<FBox, Matrix4>> out;
	out.reserve(m_draw_calls.size());
	for(auto &dc : m_draw_calls)
		if(dc.bbox)
			out.emplace_back(*dc.bbox, dc.matrix);
	return out;
}

PVertexArray RenderList::makeVao(CSpan<float3> vertices, CSpan<IColor> colors,
								 CSpan<float2> tex_coords) {
	auto pbuffer = GlBuffer::make(BufferType::array, vertices);
	DASSERT(colors.empty() || colors.size() == vertices.size());
	DASSERT(tex_coords.empty() || tex_coords.size() == vertices.size());

	vector<IColor> temp;
	if(colors.empty()) {
		// TODO: add GlBuffer::fill()
		temp.resize(vertices.size(), ColorId::white);
		colors = temp;
	}
	auto cbuffer = GlBuffer::make(BufferType::array, colors);

	auto vao = GlVertexArray::make();
	if(tex_coords) {
		auto tbuffer = GlBuffer::make(BufferType::array, tex_coords);
		vao->set({pbuffer, cbuffer, tbuffer}, defaultVertexAttribs<float3, IColor, float2>());
	} else {
		vao->set({pbuffer, cbuffer}, defaultVertexAttribs<float3, IColor>());
	}
	return vao;
}

DrawCall RenderList::makeDrawCall(CSpan<float3> vertices, CSpan<IColor> colors,
								  CSpan<float2> tex_coords) {
	return {makeVao(vertices, colors, tex_coords), PrimitiveType::triangles, vertices.size(), 0};
}

DrawCall RenderList::makeDrawCall(CSpan<ColoredTriangle> tris, Maybe<FBox> bbox) {
	vector<float3> positions;
	vector<IColor> colors;

	positions.reserve(tris.size() * 3);
	colors.reserve(tris.size() * 3);
	for(auto &tri : tris)
		for(int i = 0; i < 3; i++) {
			positions.emplace_back(tri[i]);
			colors.emplace_back(tri.colors[i]);
		}
	DrawCall out(makeVao(positions, colors), PrimitiveType::triangles, positions.size(), 0);
	out.bbox = bbox ? *bbox : enclose(positions);
	return out;
}

/*
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
		GlTexture::bind(mat.textures);
		PProgram program = mat.texture() ? tex_program : flat_program;

		ProgramBinder binder(program);
		binder.bind();
		if(mat.texture())
			binder.setUniform("tex", 0);
		binder.setUniform("proj_view_matrix", projectionMatrix() * sprite.matrix);
		binder.setUniform("mesh_color", (float4)mat.color);
		sprite_array.draw(PrimitiveType::triangles, sprite_array.size(), 0);
	}
}*/

void RenderList::clear() { m_draw_calls.clear(); }
}
