// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/material.h"
#include <map>

namespace fwk {

class MaterialSet {
  public:
	explicit MaterialSet(const Material &default_mat, std::map<string, Material> = {});
	~MaterialSet();

	auto defaultMat() const { return m_default; }
	const Material &operator[](const string &name) const;
	vector<Material> operator()(CSpan<string> names) const;
	const auto &map() const { return m_map; }

  private:
	Material m_default;
	// TODO: hash_map; or maybe even simple vector...
	std::map<string, Material> m_map;
};
}
