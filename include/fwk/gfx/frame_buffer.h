// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/render_buffer.h"
#include "fwk/gfx_base.h"

namespace fwk {

struct FrameBufferTarget {
	FrameBufferTarget() {}
	FrameBufferTarget(STexture texture) : texture(move(texture)) {}
	FrameBufferTarget(SRenderBuffer render_buffer) : render_buffer(move(render_buffer)) {}

	operator bool() const;
	TextureFormat format() const;
	int2 size() const;

	STexture texture;
	SRenderBuffer render_buffer;
};

class FrameBuffer {
  public:
	using Target = FrameBufferTarget;
	FrameBuffer(vector<Target> colors, Target depth = Target());
	FrameBuffer(Target color, Target depth = Target());
	~FrameBuffer();

	static shared_ptr<FrameBuffer> make(vector<Target> colors, Target depth = Target()) {
		return make_shared<FrameBuffer>(move(colors), move(depth));
	}

	void operator=(const FrameBuffer &) = delete;
	FrameBuffer(const FrameBuffer &) = delete;

	void bind();
	static void unbind();

	const auto &colors() const { return m_colors; }
	const Target &depth() const { return m_depth; }
	int2 size() const;

  private:
	vector<Target> m_colors;
	Target m_depth;
	uint m_id;
};
}
