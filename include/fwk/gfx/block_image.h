// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/math_base.h"
#include "fwk/pod_vector.h"
#include "fwk/sys_base.h"
#include "fwk/vulkan_base.h"

namespace fwk {

// All sizes in bytes; Returns 0 for depth/stencil formats
int blockSize(VBlockFormat);
int imageSize(VBlockFormat, int width, int height);
int imageRowSize(VBlockFormat, int width);

// RGB/RGBA image packed in 4x4 blocks
class BlockImage {
  public:
	using Format = VBlockFormat;

	BlockImage(PodVector<u8>, int2 size, VBlockFormat);
	BlockImage(const Image &, VBlockFormat);
	~BlockImage();

	CSpan<u8> data() const { return m_data; }
	Span<u8> data() { return m_data; }

	VBlockFormat format() const { return m_format; }
	int2 size() const { return m_size; }
	bool empty() const { return m_size == int2(0, 0); }

  private:
	PodVector<u8> m_data;
	int2 m_size;
	VBlockFormat m_format;
};
}
