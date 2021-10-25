// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/math_base.h"
#include "fwk/pod_vector.h"
#include "fwk/sys_base.h"

namespace fwk {

// RGB/RGBA image packed in 4x4 blocks
class CompressedImage {
  public:
	CompressedImage(PodVector<u8>, int2 size, GlFormat);
	CompressedImage(const Image &, GlFormat);
	~CompressedImage();

	CSpan<u8> data() const { return m_data; }
	Span<u8> data() { return m_data; }

	GlFormat format() const { return m_format; }
	int2 size() const { return m_size; }

  private:
	PodVector<u8> m_data;
	int2 m_size;
	GlFormat m_format;
};
}
