// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/material.h"
#include "fwk/gfx/material_set.h"

namespace fwk {

Material::Material(vector<PTexture> textures, IColor color, Flags flags, u16 custom_flags)
	: textures(move(textures)), color(color), custom_flags(custom_flags), flags(flags) {
	for(const auto &tex : textures)
		DASSERT(tex);
}

Material::Material(IColor color, Flags flags, u16 custom_flags)
	: color(color), custom_flags(custom_flags), flags(flags) {}

FWK_ORDER_BY_DEF(Material)
}
