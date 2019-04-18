// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/model_node.h"

#include "fwk/hash_map.h"
#include "fwk/sys/xml.h"

namespace fwk {

ModelNode::ModelNode() = default;
ModelNode::ModelNode(string name, Type type, const AffineTrans &trans, vector<Property> props)
	: props(move(props)), name(move(name)), trans(trans), inv_trans(inverseOrZero(trans)),
	  type(type) {}

FWK_COPYABLE_CLASS_IMPL(ModelNode)

ModelNode::PropertyMap ModelNode::propertyMap() const {
	PropertyMap out;
	for(const auto &prop : props)
		out[prop.first] = prop.second;
	return out;
}

void ModelNode::setTrans(const AffineTrans &trans) {
	this->trans = trans;
	inv_trans = inverseOrZero(trans);
}

}
