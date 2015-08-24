/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

DEFINE_ENUM(RenderBufferType, "depth", "depth_stencil");

const int internal_types[] = {
	GL_DEPTH_COMPONENT16, GL_DEPTH_STENCIL,
};
static_assert(arraySize(internal_types) == RenderBufferType::count, "");

RenderBuffer::RenderBuffer(const int2 &size, Type type) : m_size(size), m_type(type), m_id(0) {
	try {
		glGenRenderbuffers(1, &m_id);
		glBindRenderbuffer(GL_RENDERBUFFER, m_id);
		glRenderbufferStorage(GL_RENDERBUFFER, internal_types[type], m_size.x, m_size.y);
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
