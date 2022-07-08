// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx/color.h"
#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"

namespace fwk {

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

	PTexture texture() const { return textures ? textures.front() : PTexture(); }

	FWK_ORDER_BY_DECL(Material);

	// invariant: allOf(m_textures, [](auto &tex) { return tex != nullptr; })
	vector<PTexture> textures;
	IColor color;
	u16 custom_flags;
	Flags flags;
};
}
