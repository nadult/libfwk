// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"

namespace fwk {

DEFINE_ENUM(TextureFormatId, rgba, rgba_f16, rgba_f32, rgb, rgb_f16, rgb_f32, r32i, r32ui, r8, rg8,
			dxt1, dxt3, dxt5, depth16, depth24, depth32, depth32f, depth_stencil);

bool hasDepthComponent(TextureFormatId);

class TextureFormat {
  public:
	using Id = TextureFormatId;

	TextureFormat(int internal, int format, int type);
	TextureFormat(Id id = Id::rgba) : m_id(id) {}

	Id id() const { return m_id; }
	int glInternal() const;
	int glFormat() const;
	int glType() const;
	bool isCompressed() const;

	int bytesPerPixel() const;

	int evalImageSize(int width, int height) const;
	int evalLineSize(int width) const;

	bool isSupported() const;

	bool operator==(const TextureFormat &rhs) const { return m_id == rhs.m_id; }

  private:
	Id m_id;
};
}
