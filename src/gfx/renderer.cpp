/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <algorithm>

namespace fwk {

static const char *fragment_shader_tex_src =
	"#version 100\n"
	"uniform sampler2D tex; 											\n"
	"varying lowp vec4 color;											\n"
	"varying mediump vec2 tex_coord;									\n"
	"void main() {														\n"
	"	gl_FragColor = color * texture2D(tex, tex_coord);				\n"
	"}																	\n";

static const char *fragment_shader_flat_src =
	"#version 100\n"
	"varying lowp vec4 color;											\n"
	"void main() {														\n"
	"	gl_FragColor = color;											\n"
	"}																	\n";

static const char *vertex_shader_src =
	"#version 100\n"
	"uniform mat4 proj_view_matrix;										\n"
	"uniform vec4 mesh_color;											\n"
	"attribute vec3 in_pos;												\n"
	"attribute vec4 in_color;											\n"
	"attribute vec2 in_tex_coord;										\n"
	"varying vec2 tex_coord;  											\n"
	"varying vec4 color;  												\n"
	"void main() {														\n"
	"	gl_Position = proj_view_matrix * vec4(in_pos, 1.0);				\n"
	"	tex_coord = in_tex_coord;										\n"
	"	color = in_color * mesh_color;									\n"
	"} 																	\n";

struct ProgramFactory {
	PProgram operator()(const string &name) const {
		bool with_texture = name == "with_texture";
		Shader vertex_shader(Shader::tVertex), fragment_shader(Shader::tFragment);
		vertex_shader.setSource(vertex_shader_src);
		fragment_shader.setSource(with_texture ? fragment_shader_tex_src
											   : fragment_shader_flat_src);

		return make_cow<Program>(vertex_shader, fragment_shader,
								 vector<string>{"in_pos", "in_color", "in_tex_coord"});
	}
};

Renderer::Renderer(const Matrix4 &projection_matrix) : MatrixStack(projection_matrix) {
	static ResourceManager<Program, ProgramFactory> mgr;
	m_tex_program = mgr["with_texture"];
	m_flat_program = mgr["without_texture"];
}

void Renderer::addDrawCall(const DrawCall &draw_call, PMaterial material, const Matrix4 &matrix) {
	DASSERT(material);
	m_instances.emplace_back(Instance{fullMatrix() * matrix, std::move(material), draw_call});
}

void Renderer::addLines(Range<const float3> verts, Color color, const Matrix4 &matrix) {
	DASSERT(verts.size() % 2 == 0);

	m_lines.emplace_back(
		LineInstance{fullMatrix() * matrix, (int)m_line_positions.size(), verts.size()});
	m_line_positions.insert(m_line_positions.end(), begin(verts), end(verts));
	m_line_colors.resize(m_line_colors.size() + verts.size(), color);
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

void Renderer::addSprite(TRange<const float3, 4> verts, TRange<const float2, 4> tex_coords,
						 PMaterial material, const Matrix4 &matrix) {
	DASSERT(material);
	SpriteInstance new_sprite;
	new_sprite.matrix = fullMatrix() * matrix;
	new_sprite.material = std::move(material);
	std::copy(begin(verts), begin(verts) + 4, begin(new_sprite.verts));
	std::copy(begin(tex_coords), begin(tex_coords) + 4, begin(new_sprite.tex_coords));
	m_sprites.push_back(new_sprite);
}

static void bindTextures(const vector<PTexture> &textures) {
	static int max_bind = 0;

	for(int n = 0; n < (int)textures.size(); n++) {
		glActiveTexture(GL_TEXTURE0 + n);
		textures[n]->bind();
	}
	for(int n = (int)textures.size(); n < max_bind; n++) {
		glActiveTexture(GL_TEXTURE0 + n);
		glDisable(GL_TEXTURE_2D);
	}
	max_bind = (int)textures.size();
	glActiveTexture(GL_TEXTURE0);
}

namespace {

	struct DeviceConfig {
		DeviceConfig() : flags(~0u) { update(0); }

		void update(uint new_flags) {
			if((new_flags & Material::flag_blended) != (flags & Material::flag_blended)) {
				GfxDevice::setBlendingMode(new_flags & Material::flag_blended
											   ? GfxDevice::bmNormal
											   : GfxDevice::bmDisabled);
			}
			if((new_flags & Material::flag_two_sided) != (flags & Material::flag_two_sided)) {
				if(new_flags & Material::flag_two_sided)
					glDisable(GL_CULL_FACE);
				else
					glEnable(GL_CULL_FACE);
			}
			if((new_flags & Material::flag_wire) != (flags & Material::flag_wire)) {
#ifndef FWK_TARGET_HTML5
				// TODO: if it's not supported on all of the targets then remove
				glPolygonMode(GL_FRONT_AND_BACK,
							  new_flags & Material::flag_wire ? GL_LINE : GL_FILL);
#endif
			}
			if((new_flags & Material::flag_clear_depth) && !(flags & Material::flag_clear_depth)) {
				GfxDevice::clearDepth(1.0f);
			}

			flags = new_flags;
		}

		uint flags;
	};
}

void Renderer::render() {
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(1);

	DeviceConfig dev_config;

	for(const auto &instance : m_instances) {
		auto material = instance.material;

		bindTextures(material->textures());
		PProgram program = material->program();
		if(!program)
			program = material->texture() ? m_tex_program : m_flat_program;

		ProgramBinder binder(program);
		binder.bind();
		binder.setUniform("proj_view_matrix", instance.matrix);
		binder.setUniform("mesh_color", (float4)material->color());
		dev_config.update(material->flags());

		instance.draw_call.issue();
	}

	dev_config.update(Material::flag_blended | Material::flag_two_sided);
	glDepthMask(0);
	{
		vector<float3> positions;
		vector<float2> tex_coords;
		for(const auto &sprite : m_sprites) {
			positions.insert(end(positions), begin(sprite.verts), end(sprite.verts));
			tex_coords.insert(end(tex_coords), begin(sprite.tex_coords), end(sprite.tex_coords));
		}

		VertexArray sprite_array({make_cow<VertexBuffer>(positions),
								  VertexArraySource(Color::white),
								  make_cow<VertexBuffer>(tex_coords)});

		// TODO: transform to screen space, divide into regions, sort each region
		for(int n = 0; n < (int)m_sprites.size(); n++) {
			const auto &instance = m_sprites[n];

			auto material = instance.material;
			bindTextures(material->textures());
			PProgram program = material->program();
			if(!program)
				program = material->texture() ? m_tex_program : m_flat_program;

			ProgramBinder binder(program);
			binder.bind();
			if(material->texture())
				binder.setUniform("tex", 0);
			binder.setUniform("proj_view_matrix", instance.matrix);
			binder.setUniform("mesh_color", (float4)material->color());
			sprite_array.draw(PrimitiveType::triangle_strip, 4, n * 4);
		}
	}

	glDepthMask(1);
	VertexArray line_array({make_cow<VertexBuffer>(m_line_positions),
							make_cow<VertexBuffer>(m_line_colors),
							VertexArraySource(float2(0, 0))});
	DTexture::unbind();
	ProgramBinder binder(m_flat_program);
	binder.bind();
	binder.setUniform("mesh_color", float4(1, 1, 1, 1));
	for(const auto &instance : m_lines) {
		binder.setUniform("proj_view_matrix", instance.matrix);
		line_array.draw(PrimitiveType::lines, instance.count, instance.first);
	}

	m_instances.clear();
	m_sprites.clear();
	m_line_positions.clear();
	m_line_colors.clear();
	m_lines.clear();

	DTexture::unbind();
}
}
