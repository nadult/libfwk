/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

static float s_default_matrix[16];
static int2 s_viewport_size;

void initViewport(int2 size) {
	s_viewport_size = size;
	glViewport(0, 0, size.x, size.y);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, size.x, 0, size.y, -1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(0.0f, size.y, 0.0f);
	glScalef(1.0f, -1.0f, 1.0f);

	glGetFloatv(GL_MODELVIEW_MATRIX, s_default_matrix);
}

void lookAt(int2 pos) {
	glLoadMatrixf(s_default_matrix);
	glTranslatef(-pos.x, -pos.y, 0.0f);
}

void drawQuad(int2 pos, int2 size, Color color) {
	glBegin(GL_QUADS);

	glColor(color);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2i(pos.x, pos.y);
	glTexCoord2f(1.0f, 0.0f);
	glVertex2i(pos.x + size.x, pos.y);
	glTexCoord2f(1.0f, 1.0f);
	glVertex2i(pos.x + size.x, pos.y + size.y);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2i(pos.x, pos.y + size.y);

	glEnd();
}

void drawQuad(int2 pos, int2 size, const float2 &uv0, const float2 &uv1, Color color) {
	glBegin(GL_QUADS);

	glColor(color);
	glTexCoord2f(uv0.x, uv0.y);
	glVertex2i(pos.x, pos.y);
	glTexCoord2f(uv1.x, uv0.y);
	glVertex2i(pos.x + size.x, pos.y);
	glTexCoord2f(uv1.x, uv1.y);
	glVertex2i(pos.x + size.x, pos.y + size.y);
	glTexCoord2f(uv0.x, uv1.y);
	glVertex2i(pos.x, pos.y + size.y);

	glEnd();
}

void drawQuad(const FRect &rect, const FRect &uv_rect, Color colors[4]) {
	glBegin(GL_QUADS);

	glColor(colors[0]);
	glTexCoord2f(uv_rect.min.x, uv_rect.min.y);
	glVertex2f(rect.min.x, rect.min.y);
	glColor(colors[1]);
	glTexCoord2f(uv_rect.max.x, uv_rect.min.y);
	glVertex2f(rect.max.x, rect.min.y);
	glColor(colors[2]);
	glTexCoord2f(uv_rect.max.x, uv_rect.max.y);
	glVertex2f(rect.max.x, rect.max.y);
	glColor(colors[3]);
	glTexCoord2f(uv_rect.min.x, uv_rect.max.y);
	glVertex2f(rect.min.x, rect.max.y);

	glEnd();
}

void drawQuad(const FRect &rect, const FRect &uv_rect, Color color) {
	glBegin(GL_QUADS);

	glColor(color);
	glTexCoord2f(uv_rect.min.x, uv_rect.min.y);
	glVertex2f(rect.min.x, rect.min.y);
	glTexCoord2f(uv_rect.max.x, uv_rect.min.y);
	glVertex2f(rect.max.x, rect.min.y);
	glTexCoord2f(uv_rect.max.x, uv_rect.max.y);
	glVertex2f(rect.max.x, rect.max.y);
	glTexCoord2f(uv_rect.min.x, uv_rect.max.y);
	glVertex2f(rect.min.x, rect.max.y);

	glEnd();
}

void drawLine(int2 p1, int2 p2, Color color) {
	glBegin(GL_LINES);

	glColor(color);
	glVertex2i(p1.x, p1.y);
	glVertex2i(p2.x, p2.y);

	glEnd();
}

void drawRect(const IRect &rect, Color col) {
	glBegin(GL_LINE_STRIP);
	glColor(col);

	glVertex2i(rect.min.x, rect.min.y);
	glVertex2i(rect.max.x, rect.min.y);
	glVertex2i(rect.max.x, rect.max.y);
	glVertex2i(rect.min.x, rect.max.y);
	glVertex2i(rect.min.x, rect.min.y);

	glEnd();
}

void clear(Color color) {
	float4 col = color;
	glClearColor(col.x, col.y, col.z, col.w);
	glClear(GL_COLOR_BUFFER_BIT);
}

void setBlendingMode(BlendingMode mode) {
	if(mode == bmDisabled)
		glDisable(GL_BLEND);
	else
		glEnable(GL_BLEND);

	if(mode == bmNormal)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static IRect s_scissor_rect = IRect::empty();

void setScissorRect(const IRect &rect) {
	s_scissor_rect = rect;
	glScissor(rect.min.x, s_viewport_size.y - rect.max.y, rect.width(), rect.height());
	testGlError("glScissor");
}

const IRect getScissorRect() { return s_scissor_rect; }

void setScissorTest(bool is_enabled) {
	if(is_enabled)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
}
}
