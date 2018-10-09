// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_framebuffer.h"

#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/gl_renderbuffer.h"
#include "fwk/gfx/gl_texture.h"
#include "fwk/gfx/opengl.h"
#include "fwk/math/box.h"
#include "fwk/sys/assert.h"
#include <numeric>

namespace fwk {

FramebufferTarget::operator bool() const { return texture || rbo; }
GlFormat FramebufferTarget::format() const {
	return texture ? texture->format() : rbo ? rbo->format() : GlFormat();
}
int2 FramebufferTarget::size() const {
	return texture ? texture->size() : rbo ? rbo->size() : int2();
}

GL_CLASS_IMPL(GlFramebuffer)

void GlFramebuffer::attach(int type, const Target &target) {
	DASSERT(target);
	if(target.rbo)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, type, GL_RENDERBUFFER, target.rbo.id());
	else
		glFramebufferTexture2D(GL_FRAMEBUFFER, type, target.texture->glType(), target.texture.id(),
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

PFramebuffer GlFramebuffer::make(CSpan<Target> colors, Target depth) {
	PFramebuffer ref(storage.make());

	DASSERT_LT(colors.size(), max_colors);
	fwk::copy(ref->m_colors, colors);
	ref->m_num_colors = colors.size();
	ref->m_depth = depth;

	if(colors) {
		ref->m_size = colors.front().size();
		for(const auto &color : colors)
			DASSERT(color && color.size() == ref->m_size);
		DASSERT(!depth || depth.size() == ref->m_size);
		// TODO: properly test that number of attachments is <= GL_MAX_COLOR_ATTACHMENTS
	} else if(depth) {
		ref->m_size = depth.size();
	}
	if(depth)
		DASSERT(hasDepthComponent(depth.format()));

	glBindFramebuffer(GL_FRAMEBUFFER, ref.id());

	if(depth) {
		auto format = depth.format();
		bool has_stencil = format == GlFormat::depth_stencil;
		attach(has_stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, depth);
	}

	for(int n = 0; n < ref->m_num_colors; n++)
		attach(GL_COLOR_ATTACHMENT0 + n, ref->m_colors[n]);
	auto ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(ret != GL_FRAMEBUFFER_COMPLETE)
		FATAL("Error while initializing framebuffer: %s", fboError(ret));

	if(colors) {
		GLenum indices[max_colors];
		std::iota(begin(indices), end(indices), GL_COLOR_ATTACHMENT0);
		glDrawBuffers(colors.size(), indices);
	} else {
		glDrawBuffer(GL_NONE);
	}
	unbind();
	return ref;
}

PFramebuffer GlFramebuffer::make(Target color, Target depth) {
	return color ? make(cspan({move(color)}), move(depth)) : make(CSpan<Target>(), move(depth));
}

int2 GlFramebuffer::size() const { return m_size; }
void GlFramebuffer::bind() { glBindFramebuffer(GL_FRAMEBUFFER, id()); }
void GlFramebuffer::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void GlFramebuffer::copyTo(PFramebuffer dst, IRect src_rect, IRect dst_rect, FramebufferBits bits,
						   bool interpolate) {
	copy(id(), dst.id(), src_rect, dst_rect, bits, interpolate);
}

void GlFramebuffer::copy(PFramebuffer src, PFramebuffer dst, IRect src_rect, IRect dst_rect,
						 Bits bits, bool interpolate) {
	uint mask = (bits & FramebufferBit::color ? GL_COLOR_BUFFER_BIT : 0) |
				(bits & FramebufferBit::depth ? GL_DEPTH_BUFFER_BIT : 0) |
				(bits & FramebufferBit::stencil ? GL_STENCIL_BUFFER_BIT : 0);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, src.id());
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.id());
	glBlitFramebuffer(src_rect.x(), src_rect.y(), src_rect.ex(), src_rect.ey(), dst_rect.x(),
					  dst_rect.y(), dst_rect.ex(), dst_rect.ey(), mask,
					  interpolate ? GL_LINEAR : GL_NEAREST);
}
}
