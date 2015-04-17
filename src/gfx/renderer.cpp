/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <algorithm>

namespace fwk {

// static int s_gl_primitives[PrimitiveTypeId::count] = {GL_POINTS, GL_LINES, GL_TRIANGLES,
//													  GL_TRIANGLE_STRIP, GL_QUADS};

/*
void renderMesh(const Stream &stream, PTexture tex, Color color) {
	PROFILE("Renderer::renderMesh");
	if(!stream.vertex_count)
		return;
	DASSERT(stream.positions);

	glColor4ub(color.r, color.g, color.b, color.a);
	if(tex)
		tex->bind();
	else
		DTexture::unbind();

	glVertexPointer(3, GL_FLOAT, 0, stream.positions);
	glEnableClientState(GL_VERTEX_ARRAY);

	if(stream.normals) {
		glNormalPointer(GL_FLOAT, 0, stream.normals);
		glEnableClientState(GL_NORMAL_ARRAY);
	} else
		glDisableClientState(GL_NORMAL_ARRAY);

	if(stream.tex_coords) {
		glTexCoordPointer(2, GL_FLOAT, 0, stream.tex_coords);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	glDrawArrays(s_gl_primitives[stream.primitive_type], 0, stream.vertex_count);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}*/

static PProgram defaultProgram() {
	Shader vertex_shader(Shader::tVertex);
	Shader pixel_shader(Shader::tFragment);
	Loader("test.vsh") >> vertex_shader;
	Loader("test.fsh") >> pixel_shader;
	PProgram out = new Program(vertex_shader, pixel_shader);
	out->bindAttribLocation("vertex", 0);
	return out;
}

Renderer::Renderer(const IRect &viewport, const Matrix4 &view_mat, const Matrix4 &proj_mat)
	: m_viewport(viewport) {
	m_default_program = defaultProgram();
	reset(view_mat, proj_mat);
}

void Renderer::reset(const Matrix4 &view_matrix, const Matrix4 &projection_matrix) {
	m_view_matrix = view_matrix;
	m_projection_matrix = projection_matrix;
	m_inv_view_matrix = inverse(view_matrix);
	m_default_program->setUniform("modelViewMatrix", m_view_matrix);
	m_default_program->setUniform("projectionMatrix", m_projection_matrix);

	/*	m_zoom = zoom;

		m_w2s = isometricView(m_view_pos, m_zoom);
		m_s2w = inverse(m_w2s);

		m_zero_depth = mulPoint(m_w2s, screenToWorld(float2(0, 0), 0.0f)).z;
		m_s2w = inverse(m_w2s);

		g_model_view.push();
		setupView(m_w2s); // TODO
		m_frustum = Frustum(g_projection * g_model_view);
		g_model_view.pop();*/

	m_rects.clear();
}

const Frustum Renderer::frustum(const FRect &screen_rect) const {
	Frustum out;

	Ray rays[4];
	rays[0] = -screenRay(screen_rect.min);
	rays[1] = -screenRay(float2(screen_rect.min.x, screen_rect.max.y));
	rays[2] = -screenRay(screen_rect.max);
	rays[3] = -screenRay(float2(screen_rect.max.x, screen_rect.min.y));

	out[0] = Plane(rays[1].origin(), rays[0].origin(), rays[0].at(1.0f));
	out[1] = Plane(rays[3].origin(), rays[2].origin(), rays[2].at(1.0f));
	out[2] = Plane(rays[2].origin(), rays[1].origin(), rays[1].at(1.0f));
	out[3] = Plane(rays[0].origin(), rays[3].origin(), rays[3].at(1.0f));

	return out;
}

const Ray Renderer::screenRay(const float2 &screen_pos) const {
	return Ray(m_inv_view_matrix, screen_pos);
}

const float2 Renderer::worldToScreen(const float3 &pos) const {
	return mulPoint(m_view_matrix, pos).xy();
}

const float3 Renderer::screenToWorld(const float2 &screen_pos, float floor_height) const {
	Plane floor(float3(0, 1, 0), floor_height);
	Ray screen_ray = screenRay(screen_pos);
	float isect = intersection(screen_ray, floor);
	return screen_ray.at(isect);
}

void Renderer::addRect(const FRect &rect, PTexture texture, RectStyle style) {
	m_rects.emplace_back(RectInstance{rect, texture, style});
}

void Renderer::render() {
	enum { node_size = 128 };

	//	setupView(translation(m_viewport.min.x, m_viewport.min.y, -m_zero_depth - 1000.0f) * m_w2s);
	//	setDepthTest(true);

	renderRects();
	//	glDepthMask(1);
	DTexture::unbind();
}

void Renderer::renderRects() const {
	m_default_program->bind();

	vector<float3> positions(m_rects.size() * 4);
	for(int n = 0; n < (int)m_rects.size(); n++) {
		positions[n * 4 + 0] = float3(m_rects[n].rect.min.x, m_rects[n].rect.min.y, 0.0f);
		positions[n * 4 + 1] = float3(m_rects[n].rect.max.x, m_rects[n].rect.min.y, 0.0f);
		positions[n * 4 + 3] = float3(m_rects[n].rect.min.x, m_rects[n].rect.max.y, 0.0f);
		positions[n * 4 + 2] = float3(m_rects[n].rect.max.x, m_rects[n].rect.max.y, 0.0f);
	}

//	glBegin(GL_QUADS);
//	for(auto &pos : positions)
//		glVertex3f(pos.x, pos.y, pos.z);
//	glEnd();
	
	VertexArray array;
	VertexBuffer buf_pos;

	array.bind();
	buf_pos.setData(sizeof(float3) * positions.size(), &positions[0], GL_STATIC_DRAW);
	array.addAttrib(3, GL_FLOAT, 0, sizeof(float3), 0);
	
	VertexArray::unbind();
	VertexBuffer::unbind();

	//	uv.SetData(sizeof(Vec2) * data.uvs.size(), &data.uvs[0], GL_STATIC_DRAW);
	//	vao.VertexAttrib(2, 2, GL_FLOAT, 0, sizeof(Vec2), 0);

	array.bind();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, m_rects.size() * 4);
	VertexBuffer::unbind();

	Program::unbind();
}
}
