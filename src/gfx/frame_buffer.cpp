/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

FrameBuffer::FrameBuffer(STexture color_buffer, SRenderBuffer depth_buffer)
	: m_color_buffer(std::move(color_buffer)), m_depth_buffer(std::move(depth_buffer)), m_id(0) {
	DASSERT(m_color_buffer);
	DASSERT(!m_depth_buffer || m_color_buffer->size() == m_depth_buffer->size());

	try {
		glGenFramebuffers(1, &m_id);
		glBindFramebuffer(GL_FRAMEBUFFER, m_id);
		if(m_depth_buffer) {
			int type = m_depth_buffer->type() == RenderBufferType::depth_stencil
						   ? GL_DEPTH_STENCIL_ATTACHMENT
						   : GL_DEPTH_ATTACHMENT;
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, type, GL_RENDERBUFFER, m_depth_buffer->id());
		}
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
							   m_color_buffer->id(), 0);
		if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			THROW("Error while initializing framebuffer");
	} catch(...) {
		glDeleteFramebuffers(1, &m_id);
		throw;
	}
}

FrameBuffer::~FrameBuffer() { glDeleteFramebuffers(1, &m_id); }

void FrameBuffer::bind() { glBindFramebuffer(GL_FRAMEBUFFER, m_id); }

void FrameBuffer::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
}
