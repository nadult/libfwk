/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <unordered_map>

namespace fwk {

ModelNode::ModelNode(const ModelNode *parent, const string &name, const AffineTrans &trans,
					 int mesh_id)
	: m_name(name), m_trans(trans), m_parent(parent), m_mesh_id(mesh_id), m_id(-1) {}

const ModelNode *ModelNode::root() const {
	const auto *node = this;
	while(node && node->m_parent)
		node = node->m_parent;
	return node;
}

AffineTrans ModelNode::globalTrans() const {
	return m_parent ? m_parent->globalTrans() * m_trans : m_trans;
}

struct ModelTree::Map : public std::unordered_map<string, const ModelNode *> {};

ModelTree::ModelTree()
	: m_map(make_unique<Map>()), m_root(nullptr, "", AffineTrans()), m_is_dirty(false) {
	m_map->emplace("", &m_root);
}

ModelTree::~ModelTree() = default;

bool ModelTree::addNodeName(const ModelNode *node) {
	if(findNode(node->name()))
		return false;
	m_map->emplace(node->name(), node);
	return true;
}

void ModelTree::removeNode(const ModelNode *node) {
	DASSERT(node && node != &m_root && node->root() == &m_root);
	auto &container = const_cast<ModelNode *>(node->m_parent)->m_children;
	for(auto it = begin(container); it != end(container); ++it)
		if(it->get() == node) {
			container.erase(it);
			break;
		}
	m_is_dirty = true;
}

const ModelNode *ModelTree::findNode(const string &name) const {
	if(m_is_dirty)
		updateNodeIds();
	auto it = m_map->find(name);
	return it == m_map->end() ? nullptr : it->second;
}

static void dfs(vector<const ModelNode *> &out, const ModelNode *node) {
	out.push_back(node);
	for(const auto &child : node->children())
		dfs(out, child.get());
}

void ModelTree::updateNodeIds() const {
	if(m_is_dirty) {
		m_all_nodes.clear();
		dfs(m_all_nodes, &m_root);
		for(int n = 0; n < (int)m_all_nodes.size(); n++)
			const_cast<ModelNode *>(m_all_nodes[n])->m_id = n;
		m_is_dirty = false;
	}
}

/*
void ModelTree::join(const ModelNode *target, const ModelNode *source) {
	DASSERT(target && source);
	DASSERT(target->root() == root() && source->root() != root());

	target->
}*/

/*
vector<int> ModelTree::findNodes(const vector<string> &names) const {
	vector<int> out;
	for(const auto &name : names)
		out.push_back(findNode(name));
	return out;
}

int ModelTree::findNode(const string &name) const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		if(m_nodes[n].name == name)
			return n;
	return -1;
}

void ModelTree::removeNode(int node_id) {
	DASSERT(node_id != -1);

}

int ModelTree::addMeshNode(const string &name, const AffineTrans &trans, int mesh_id, int parent_id)
{
	DASSERT(parent_id >= -1 && parent_id < (int)m_nodes.size());

	int node_id = -1;
	if(m_free_nodes.empty()) {
		node_id = (int)m_nodes.size();
		m_nodes.push_back(Node{});
	}
	else {
		node_id = m_free_nodes.back();
		m_free_nodes.pop_back();
	}

	auto &node = m_nodes[node_id];
	node = Node(Node{name, trans, node_id, parent_id, -1, vector<int>()});
	if(parent_id != -1)
		m_nodes[parent_id].children_ids.emplace_back(node_id);
	return node_id;
}


int ModelTree::addMeshMode(

void ModelTree::updateNodeIndices() {

}*/
}
