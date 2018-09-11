// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/render_buffer.h"
#include "fwk/gfx_base.h"

namespace fwk {

struct FramebufferTarget {
	FramebufferTarget() {}
	FramebufferTarget(STexture texture) : texture(move(texture)) {}
	FramebufferTarget(SRenderBuffer render_buffer) : render_buffer(move(render_buffer)) {}

	operator bool() const;
	TextureFormat format() const;
	int2 size() const;

	STexture texture;
	SRenderBuffer render_buffer;
};

DEFINE_ENUM(FramebufferElement, color, depth, stencil);
using FramebufferBits = EnumFlags<FramebufferElement>;

class Framebuffer {
  public:
	static constexpr int max_color_attachments = 8;

	using Target = FramebufferTarget;
	Framebuffer(vector<Target> colors, Target depth = Target());
	Framebuffer(Target color, Target depth = Target());
	~Framebuffer();

	static shared_ptr<Framebuffer> make(vector<Target> colors, Target depth = Target()) {
		return make_shared<Framebuffer>(move(colors), move(depth));
	}

	void operator=(const Framebuffer &) = delete;
	Framebuffer(const Framebuffer &) = delete;

	void bind();
	static void unbind();

	void copyTo(const Framebuffer *, IRect src_rect, IRect dst_rect, FramebufferBits,
				bool interpolate);
	static void copy(int src_id, int dst_id, IRect src_rect, IRect dst_rect, FramebufferBits,
					 bool interpolate);

	const auto &colors() const { return m_colors; }
	const Target &depth() const { return m_depth; }
	int2 size() const;

  private:
	vector<Target> m_colors;
	int2 m_size;
	Target m_depth;
	uint m_id;
};
}
