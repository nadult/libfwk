// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/quat.h"

namespace fwk {

class AffineTrans {
  public:
	explicit AffineTrans(const Matrix4 &);
	AffineTrans(const float3 &pos, const float3 &scale, const Quat &rot = Quat())
		: translation(pos), scale(scale), rotation(rot) {}
	AffineTrans(const float3 &pos = {}, float scale = 1.0f, const Quat &rot = Quat())
		: translation(pos), scale(scale), rotation(rot) {}
	operator Matrix4() const;

	static Ex<AffineTrans> load(CXmlNode);
	void save(XmlNode) const;

	FWK_ORDER_BY(AffineTrans, translation, scale, rotation);

	float3 translation;
	float3 scale;
	Quat rotation;
};

AffineTrans operator*(const AffineTrans &, const AffineTrans &);
AffineTrans lerp(const AffineTrans &, const AffineTrans &, float t);
}
