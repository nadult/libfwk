// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

struct FramebufferTarget {
	FramebufferTarget() {}
	FramebufferTarget(None) {}
	FramebufferTarget(PTexture tex) : texture(move(tex)) {}
	FramebufferTarget(PRenderbuffer rbo) : rbo(move(rbo)) {}

	operator bool() const;
	GlFormat format() const;
	int2 size() const;

	PTexture texture;
	PRenderbuffer rbo;
};

DEFINE_ENUM(FramebufferBit, color, depth, stencil);
using FramebufferBits = EnumFlags<FramebufferBit>;

class GlFramebuffer {
	GL_CLASS_DECL(GlFramebuffer)
  public:
	using Target = FramebufferTarget;
	using Bits = FramebufferBits;
	static constexpr int max_colors = 8;

	static PFramebuffer make(CSpan<Target> colors, Target depth = {});
	static PFramebuffer make(Target color, Target depth = {});

	void bind();
	static void unbind();

	void copyTo(PFramebuffer, IRect src_rect, IRect dst_rect, FramebufferBits, bool interpolate);
	static void copy(PFramebuffer src, PFramebuffer dst, IRect src_rect, IRect dst_rect, Bits,
					 bool interpolate);

	int numColors() const { return m_num_colors; }
	CSpan<Target> colors() const { return {m_colors, m_num_colors}; }
	const Target &depth() const { return m_depth; }
	int2 size() const;

  private:
	static void attach(int type, const Target &);

	Target m_colors[max_colors];
	Target m_depth;
	int m_num_colors = 0;
	int2 m_size;
};
}
