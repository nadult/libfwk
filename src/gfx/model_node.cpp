// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/model_node.h"

#include "fwk/hash_map.h"
#include "fwk/sys/xml.h"

namespace fwk {

ModelNode::ModelNode(const string &name, Type type, const AffineTrans &trans, PMesh mesh,
					 vector<Property> props)
	: m_properties(move(props)), m_name(name), m_trans(trans), m_inv_trans(inverse(trans)),
	  m_mesh(move(mesh)), m_type(type), m_id(-1), m_parent(nullptr) {}

ModelNode::ModelNode(const ModelNode &rhs)
	: m_properties(rhs.m_properties), m_name(rhs.m_name), m_trans(rhs.m_trans),
	  m_inv_trans(rhs.m_inv_trans), m_mesh(rhs.m_mesh), m_id(-1), m_parent(nullptr) {
	for(auto &child : rhs.m_children) {
		auto child_clone = child->clone();
		child_clone->m_parent = this;
		m_children.emplace_back(move(child_clone));
	}
}

ModelNode::PropertyMap ModelNode::propertyMap() const {
	PropertyMap out;
	for(const auto &prop : m_properties)
		out[prop.first] = prop.second;
	return out;
}

void ModelNode::addChild(PModelNode node) {
	node->m_parent = this;
	m_children.emplace_back(move(node));
}

PModelNode ModelNode::removeChild(const ModelNode *child_to_remove) {
	for(auto it = begin(m_children); it != end(m_children); ++it)
		if(it->get() == child_to_remove) {
			auto child = move(*it);
			m_children.erase(it);
			return child;
		}
	return PModelNode();
}

PModelNode ModelNode::clone() const { return make_unique<ModelNode>(*this); }

void ModelNode::setTrans(const AffineTrans &trans) {
	m_trans = trans;
	m_inv_trans = inverse(trans);
}

void ModelNode::dfs(vector<ModelNode *> &out) {
	out.emplace_back(this);
	for(const auto &child : m_children)
		child->dfs(out);
}

const ModelNode *ModelNode::find(const string &name, bool recursive) const {
	for(const auto &child : m_children) {
		if(child->name() == name)
			return child.get();
		if(recursive)
			if(auto ret = child->find(name, true))
				return ret;
	}

	return nullptr;
}

const ModelNode *ModelNode::root() const {
	const auto *node = this;
	while(node && node->m_parent)
		node = node->m_parent;
	return node;
}

bool ModelNode::isDescendant(const ModelNode *test_ancestor) const {
	auto ancestor = m_parent;
	while(ancestor) {
		if(ancestor == test_ancestor)
			return true;
		ancestor = ancestor->m_parent;
	}
	return false;
}

Matrix4 ModelNode::globalTrans() const {
	return m_parent ? m_parent->globalTrans() * m_trans : m_trans;
}

Matrix4 ModelNode::invGlobalTrans() const {
	return m_parent ? m_inv_trans * m_parent->invGlobalTrans() : m_inv_trans;
}

bool ModelNode::join(const ModelNode *other, const string &name) {
	if(m_name == name) {
		m_children.clear();
		for(auto &child : other->m_children)
			m_children.emplace_back(child->clone());
		m_mesh = other->m_mesh;
		return true;
	}

	if(const auto *cnode = find(name))
		return const_cast<ModelNode *>(cnode)->join(other, name);
	return false;
}
}
