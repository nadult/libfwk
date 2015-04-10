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

void lookAt(const float2 &pos) {
	glLoadMatrixf(s_default_matrix);
	glTranslatef(-pos.x, -pos.y, 0.0f);
}

void drawRect(const FRect &rect, const RectStyle &style) {
	drawRect(rect, FRect(0, 0, 1, 1), style);
}

void drawRect(const FRect &rect, const FRect &uv_rect, const RectStyle &style) {
	if(style.fill_color != Color::transparent) {
		glBegin(GL_QUADS);
		glColor(style.fill_color);
		glTexCoord2f(uv_rect.min.x, uv_rect.min.y);
		glVertex2i(rect.min.x, rect.min.y);
		glTexCoord2f(uv_rect.max.x, uv_rect.min.y);
		glVertex2i(rect.max.x, rect.min.y);
		glTexCoord2f(uv_rect.max.x, uv_rect.max.y);
		glVertex2i(rect.max.x, rect.max.y);
		glTexCoord2f(uv_rect.min.x, uv_rect.max.y);
		glVertex2i(rect.min.x, rect.max.y);
		glEnd();
	}
	if(style.border_color != Color::transparent) {
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_LINE_STRIP);
		glColor(style.border_color);
		glTexCoord2f(uv_rect.min.x, uv_rect.min.y);
		glVertex2f(rect.min.x, rect.min.y);
		glTexCoord2f(uv_rect.max.x, uv_rect.min.y);
		glVertex2f(rect.max.x, rect.min.y);
		glTexCoord2f(uv_rect.max.x, uv_rect.max.y);
		glVertex2f(rect.max.x, rect.max.y);
		glTexCoord2f(uv_rect.min.x, uv_rect.max.y);
		glVertex2f(rect.min.x, rect.max.y);
		glTexCoord2f(uv_rect.min.x, uv_rect.min.y);
		glVertex2f(rect.min.x, rect.min.y);
		glEnd();
		glEnable(GL_TEXTURE_2D);
	}
}

void drawLine(const float2 &start, const float2 &end, Color color) {
	glBegin(GL_LINES);
	glColor(color);
	glVertex2f(start.x, start.y);
	glVertex2f(end.x, end.y);
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
