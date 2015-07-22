/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <unordered_map>

namespace fwk {

ModelNode::ModelNode(const string &name, const AffineTrans &trans, PMesh mesh)
	: m_name(name), m_trans(trans), m_mesh(std::move(mesh)), m_id(-1), m_parent(nullptr) {}

ModelNode::ModelNode(const ModelNode &rhs)
	: m_name(rhs.m_name), m_trans(rhs.m_trans), m_mesh(rhs.m_mesh), m_id(-1), m_parent(nullptr) {
	for(auto &child : rhs.m_children) {
		auto child_clone = child->clone();
		child_clone->m_parent = this;
		m_children.emplace_back(std::move(child_clone));
	}
}

void ModelNode::addChild(PModelNode node) {
	node->m_parent = this;
	m_children.emplace_back(std::move(node));
}

PModelNode ModelNode::removeChild(const ModelNode *child_to_remove) {
	for(auto it = begin(m_children); it != end(m_children); ++it)
		if(it->get() == child_to_remove) {
			auto child = std::move(*it);
			m_children.erase(it);
			return child;
		}
	return PModelNode();
}

PModelNode ModelNode::clone() const { return make_unique<ModelNode>(*this); }

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

AffineTrans ModelNode::globalTrans() const {
	return m_parent ? m_parent->globalTrans() * m_trans : m_trans;
}
}
