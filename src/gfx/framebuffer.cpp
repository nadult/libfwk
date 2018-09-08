// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/framebuffer.h"

#include "fwk/gfx/dtexture.h"
#include "fwk/gfx/opengl.h"
#include <numeric>

namespace fwk {

FramebufferTarget::operator bool() const { return texture || render_buffer; }
TextureFormat FramebufferTarget::format() const {
	return texture ? texture->format() : render_buffer ? render_buffer->format() : TextureFormat();
}
int2 FramebufferTarget::size() const {
	return texture ? texture->size() : render_buffer ? render_buffer->size() : int2();
}

static void attach(int type, const FramebufferTarget &target) {
	DASSERT(target);
	if(target.render_buffer)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, type, GL_RENDERBUFFER,
								  target.render_buffer->id());
	else
		glFramebufferTexture2D(GL_FRAMEBUFFER, type, target.texture->glType(), target.texture->id(),
							   0);
}

static const char *fboError(int error_code) {
	switch(error_code) {
#define CASE(code)                                                                                 \
	case code:                                                                                     \
		return #code;
		CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
		CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
		CASE(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
		CASE(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
		CASE(GL_FRAMEBUFFER_UNSUPPORTED)
		CASE(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
		CASE(GL_TEXTURE_FIXED_SAMPLE_LOCATIONS)
		CASE(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)
#undef CASE
	default:
		break;
	}
	return "UNKNOWN";
}

Framebuffer::Framebuffer(vector<Target> colors, Target depth)
	: m_colors(move(colors)), m_depth(move(depth)), m_id(0) {
	PASSERT_GFX_THREAD();

	if(m_colors) {
		m_size = m_colors.front().size();
		for(const auto &color : m_colors)
			DASSERT(color && color.size() == m_size);
		DASSERT(!m_depth || m_depth.size() == m_size);
		DASSERT(m_colors.size() <= max_color_attachments);
		// TODO: properly test that number of attachments is <= GL_MAX_COLOR_ATTACHMENTS
	} else if(m_depth) {
		m_size = m_depth.size();
		DASSERT(hasDepthComponent(m_depth.format().id()));
	}

	{
		glGenFramebuffers(1, &m_id);
		glBindFramebuffer(GL_FRAMEBUFFER, m_id);

		if(m_depth) {
			auto format = m_depth.format();
			bool has_stencil = format == TextureFormatId::depth_stencil;
			attach(has_stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, m_depth);
		}

		for(int n = 0; n < m_colors.size(); n++)
			attach(GL_COLOR_ATTACHMENT0 + n, m_colors[n]);

		auto ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(ret != GL_FRAMEBUFFER_COMPLETE)
			FATAL("Error while initializing framebuffer: %s", fboError(ret));

		if(m_colors) {
			GLenum indices[max_color_attachments];
			std::iota(begin(indices), end(indices), GL_COLOR_ATTACHMENT0);
			glDrawBuffers(m_colors.size(), indices);
		} else {
			glDrawBuffer(GL_NONE);
		}
	}

	// TODO: finally:
	// glDeleteFramebuffers(1, &m_id);
	unbind();
}

Framebuffer::Framebuffer(Target color, Target depth)
	: Framebuffer(vector<Target>{move(color)}, move(depth)) {}

Framebuffer::~Framebuffer() {
	PASSERT_GFX_THREAD();
	glDeleteFramebuffers(1, &m_id);
}

int2 Framebuffer::size() const { return m_size; }
void Framebuffer::bind() { glBindFramebuffer(GL_FRAMEBUFFER, m_id); }
void Framebuffer::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
}
