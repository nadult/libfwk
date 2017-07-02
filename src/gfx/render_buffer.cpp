// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

RenderBuffer::RenderBuffer(Format format, const int2 &size)
	: m_size(size), m_format(format), m_id(0) {
	try {
		glGenRenderbuffers(1, &m_id);
		glBindRenderbuffer(GL_RENDERBUFFER, m_id);
		glRenderbufferStorage(GL_RENDERBUFFER, format.glInternal(), m_size.x, m_size.y);
		testGlError("glRenderBufferStorage");
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	} catch(...) {
		glDeleteRenderbuffers(1, &m_id);
		throw;
	}
}

RenderBuffer::~RenderBuffer() {
	if(m_id)
		glDeleteRenderbuffers(1, &m_id);
}
}
