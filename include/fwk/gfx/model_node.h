// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/mesh.h"
#include "fwk/gfx_base.h"
#include "fwk/hash_map.h"
#include "fwk/math/affine_trans.h"
#include "fwk/math/matrix4.h"
#include "fwk/sys/immutable_ptr.h"

namespace fwk {

DEFINE_ENUM(ModelNodeType, generic, mesh, armature, bone, empty);

class ModelNode {
  public:
	using Type = ModelNodeType;

	using Property = Pair<string, string>;
	using PropertyMap = HashMap<string, string>;

	ModelNode();
	explicit ModelNode(string name, Type, const AffineTrans & = {}, vector<Property> = {});
	FWK_COPYABLE_CLASS(ModelNode);

	PropertyMap propertyMap() const;

	void setTrans(const AffineTrans &trans);

	vector<int> children_ids;
	vector<Property> props;
	string name;
	AffineTrans trans;
	Matrix4 inv_trans = Matrix4::identity();
	Type type = Type::generic;
	int id = -1, parent_id = -1, mesh_id = -1;
};
}
