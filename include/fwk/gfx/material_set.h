// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/fwd_member.h"
#include "fwk/gfx/material.h"

namespace fwk {

class MaterialSet {
  public:
	MaterialSet(const Material &default_mat, HashMap<string, Material>);
	explicit MaterialSet(const Material &default_mat);
	FWK_COPYABLE_CLASS(MaterialSet);

	auto defaultMat() const { return m_default; }
	const Material &operator[](const string &name) const;
	vector<Material> operator()(CSpan<string> names) const;
	const HashMap<string, Material> &map() const;

  private:
	Material m_default;
	FwdMember<HashMap<string, Material>> m_map;
};
}
