// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx_base.h"
#include "fwk/sys/immutable_ptr.h"

namespace fwk {

class SimpleMaterial {
  public:
	SimpleMaterial(STexture texture, FColor color = ColorId::white)
		: m_texture(texture), m_color(color) {}
	SimpleMaterial(PTexture texture, FColor color = ColorId::white)
		: m_texture(texture), m_color(color) {}
	SimpleMaterial(FColor color = ColorId::white) : m_color(color) {}

	shared_ptr<const DTexture> texture() const { return m_texture; }
	FColor color() const { return m_color; }

  private:
	shared_ptr<const DTexture> m_texture;
	FColor m_color;
};

DEFINE_ENUM(MaterialOpt, blended, two_sided, clear_depth, ignore_depth);
using MaterialFlags = EnumFlags<MaterialOpt>;

// TODO: FlatMaterial (without textures)

class Material {
  public:
	using Flags = MaterialFlags;

	explicit Material(vector<PTexture> textures, IColor color = ColorId::white, Flags flags = none,
					  u16 custom_flags = 0);
	explicit Material(IColor color, Flags flags = none, u16 custom_flags = 0);
	Material() : Material(ColorId::white) {}

	PTexture texture() const { return textures.empty() ? PTexture() : textures.front(); }

	FWK_ORDER_BY(Material, textures, color, custom_flags, flags);

	// invariant: allOf(m_textures, [](auto &tex) { return tex != nullptr; })
	vector<PTexture> textures;
	IColor color;
	u16 custom_flags;
	Flags flags;
};
}
