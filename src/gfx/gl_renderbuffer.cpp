// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_renderbuffer.h"

#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/opengl.h"
#include "fwk/math_base.h"

namespace fwk {

GL_CLASS_IMPL(GlRenderbuffer)

PRenderbuffer GlRenderbuffer::make(GlFormat format, const int2 &size) {
	PRenderbuffer ref(storage.make());
	ref->m_size = size;
	ref->m_format = format;

	glBindRenderbuffer(GL_RENDERBUFFER, ref.id());
	glRenderbufferStorage(GL_RENDERBUFFER, glInternalFormat(format), size.x, size.y);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	return ref;
}

PRenderbuffer GlRenderbuffer::make(GlFormat format, const int2 &size, int multisample_count) {
	PRenderbuffer ref(storage.make());
	ref->m_size = size;
	ref->m_format = format;

	glBindRenderbuffer(GL_RENDERBUFFER, ref.id());
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample_count, glInternalFormat(format),
									 size.x, size.y);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	return ref;
}
}
