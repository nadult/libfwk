// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/matrix4.h"
#include "fwk/sys/immutable_ptr.h"
#include "fwk/vector.h"

namespace fwk {

struct Pose : public immutable_base<Pose> {
  public:
	using NameMap = vector<Pair<string, int>>;

	Pose(vector<Matrix4> transforms = {}, NameMap = {});
	Pose(vector<Matrix4> transforms, const vector<string> &names);

	auto size() const { return m_transforms.size(); }
	const auto &transforms() const { return m_transforms; }
	const NameMap &nameMap() const { return m_name_map; }

	vector<int> mapNames(const vector<string> &) const;
	vector<Matrix4> mapTransforms(const vector<int> &mapping) const;

  private:
	NameMap m_name_map;
	vector<Matrix4> m_transforms;
};
}
