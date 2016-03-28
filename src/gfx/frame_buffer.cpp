/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <numeric>

namespace fwk {

FrameBufferTarget::operator bool() const { return texture || render_buffer; }
TextureFormat FrameBufferTarget::format() const {
	return texture ? texture->format() : render_buffer ? render_buffer->format() : TextureFormat();
}
int2 FrameBufferTarget::size() const {
	return texture ? texture->size() : render_buffer ? render_buffer->size() : int2();
}

static void attach(int type, const FrameBufferTarget &target) {
	DASSERT(target);
	if(target.render_buffer)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, type, GL_RENDERBUFFER,
								  target.render_buffer->id());
	else
		glFramebufferTexture2D(GL_FRAMEBUFFER, type, GL_TEXTURE_2D, target.texture->id(), 0);
}

FrameBuffer::FrameBuffer(vector<Target> colors, Target depth)
	: m_colors(move(colors)), m_depth(move(depth)), m_id(0) {
	DASSERT(!m_colors.empty() && m_colors.front());
	DASSERT(!m_depth || m_depth.size() == m_colors.front().size());
	for(const auto &color : m_colors)
		DASSERT(color.size() == m_colors.front().size());
	DASSERT((int)m_colors.size() <= GL_MAX_COLOR_ATTACHMENTS);

	try {
		glGenFramebuffers(1, &m_id);
		glBindFramebuffer(GL_FRAMEBUFFER, m_id);

		if(m_depth) {
			auto format = m_depth.format();
			DASSERT(isOneOf(format, TextureFormatId::depth, TextureFormatId::depth_stencil));
			bool has_stencil = format == TextureFormatId::depth_stencil;
			attach(has_stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, m_depth);
		}

		for(int n = 0; n < (int)m_colors.size(); n++)
			attach(GL_COLOR_ATTACHMENT0 + n, m_colors[n]);

		auto ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(ret != GL_FRAMEBUFFER_COMPLETE) {
			const char *err_code =
				ret == GL_FRAMEBUFFER_UNSUPPORTED
					? "\nunsupported combination of internal formats for attached images"
					: "unknown";
			THROW("Error while initializing framebuffer:%s", err_code);
		}

		GLenum indices[GL_MAX_COLOR_ATTACHMENTS];
		std::iota(begin(indices), end(indices), GL_COLOR_ATTACHMENT0);
		glDrawBuffers(m_colors.size(), indices);
	} catch(...) {
		glDeleteFramebuffers(1, &m_id);
		throw;
	}
	unbind();
}

FrameBuffer::FrameBuffer(Target color, Target depth)
	: FrameBuffer(vector<Target>{move(color)}, move(depth)) {}

FrameBuffer::~FrameBuffer() { glDeleteFramebuffers(1, &m_id); }

int2 FrameBuffer::size() const { return m_colors.front().size(); }
void FrameBuffer::bind() { glBindFramebuffer(GL_FRAMEBUFFER, m_id); }
void FrameBuffer::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
}
