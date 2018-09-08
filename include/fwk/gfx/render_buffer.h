// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/texture_format.h"
#include "fwk/math_base.h"

namespace fwk {

class RenderBuffer {
  public:
	using Format = TextureFormat;
	RenderBuffer(TextureFormat, const int2 &size);
	RenderBuffer(TextureFormat, const int2 &, int multisample_count);
	~RenderBuffer();

	void operator=(const RenderBuffer &) = delete;
	RenderBuffer(const RenderBuffer &) = delete;

	Format format() const { return m_format; }
	uint id() const { return m_id; }
	int2 size() const { return m_size; }

  private:
	int2 m_size;
	Format m_format;
	uint m_id;
};
}
