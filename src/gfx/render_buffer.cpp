// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/render_buffer.h"

#include "fwk/gfx/opengl.h"

namespace fwk {

RenderBuffer::RenderBuffer(Format format, const int2 &size)
	: m_size(size), m_format(format), m_id(0) {
	PASSERT_GFX_THREAD();

	{
		glGenRenderbuffers(1, &m_id);
		glBindRenderbuffer(GL_RENDERBUFFER, m_id);
		glRenderbufferStorage(GL_RENDERBUFFER, format.glInternal(), m_size.x, m_size.y);
		testGlError("glRenderBufferStorage");
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	// TODO: Finally:
	// glDeleteRenderbuffers(1, &m_id);
}

RenderBuffer::~RenderBuffer() {
	PASSERT_GFX_THREAD();
	if(m_id)
		glDeleteRenderbuffers(1, &m_id);
}
}
