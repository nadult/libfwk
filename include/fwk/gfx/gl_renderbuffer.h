// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

class GlRenderbuffer {
	GL_CLASS_DECL(GlRenderbuffer)
  public:
	static PRenderbuffer make(GlFormat, const int2 &size);
	static PRenderbuffer make(GlFormat, const int2 &, int multisample_count);

	GlFormat format() const { return m_format; }
	int2 size() const { return m_size; }

  private:
	int2 m_size;
	GlFormat m_format;
};
}
