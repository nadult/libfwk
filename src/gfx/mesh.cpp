/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <deque>
#include <unordered_map>

namespace fwk {

Mesh::Mesh(const aiScene &ascene) {
	Matrix4 mat(ascene.mRootNode->mTransformation[0]);

	m_meshes.reserve(ascene.mNumMeshes);
	for(uint n = 0; n < ascene.mNumMeshes; n++)
		m_meshes.emplace_back(SimpleMesh(ascene, n));

	std::deque<pair<const aiNode *, int>> queue({make_pair(ascene.mRootNode, -1)});

	while(!queue.empty()) {
		auto *anode = queue.front().first;
		int parent_id = queue.front().second;
		queue.pop_front();

		Node new_node;
		new_node.name = anode->mName.C_Str();
		// TODO: why transpose is needed?
		new_node.trans = transpose(Matrix4(anode->mTransformation[0]));
		new_node.parent_id = parent_id;

		for(uint m = 0; m < anode->mNumMeshes; m++)
			new_node.mesh_ids.emplace_back(anode->mMeshes[m]);

		parent_id = (int)m_nodes.size();
		m_nodes.emplace_back(new_node);

		for(uint c = 0; c < anode->mNumChildren; c++)
			queue.emplace_back(anode->mChildren[c], parent_id);
	}

	verifyData();
	computeBoundingBox();
}

Mesh::Mesh(const XMLNode &node) {
	XMLNode subnode = node.child("node");
	XMLNode mesh_node = node.child("mesh");

	while(subnode) {
		m_nodes.emplace_back(Node{subnode.attrib("name"), subnode.attrib<Matrix4>("trans"),
								  subnode.attrib<int>("parent_id"),
								  subnode.attrib<vector<int>>("mesh_ids")});
		subnode.next();
	}
	while(mesh_node) {
		m_meshes.emplace_back(mesh_node);
		mesh_node.next();
	}

	verifyData();
	computeBoundingBox();
}

void Mesh::verifyData() const {
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		const auto &node = m_nodes[n];
		ASSERT(node.parent_id >= -1 && node.parent_id < (int)m_nodes.size() && node.parent_id != n);
		for(auto mesh_id : node.mesh_ids)
			ASSERT(mesh_id >= 0 && mesh_id < (int)m_meshes.size());
	}
}

void Mesh::saveToXML(XMLNode xml_node) const {
	for(const auto &node : m_nodes) {
		XMLNode subnode = xml_node.addChild("node");
		subnode.addAttrib("name", subnode.own(node.name));
		subnode.addAttrib("trans", node.trans);
		subnode.addAttrib("parent_id", node.parent_id);
		subnode.addAttrib("mesh_ids", node.mesh_ids);
	}

	for(const auto &mesh : m_meshes) {
		XMLNode mesh_node = xml_node.addChild("mesh");
		mesh.saveToXML(mesh_node);
	}
}

void Mesh::computeBoundingBox() {
	m_bounding_box = m_meshes.empty() ? FBox::empty() : m_meshes.front().boundingBox();
	for(const auto &mesh : m_meshes)
		m_bounding_box = sum(m_bounding_box, mesh.boundingBox());
}

int Mesh::findNode(const string &name) const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		if(m_nodes[n].name == name)
			return n;
	return -1;
}

void Mesh::draw(Renderer &out, const Material &material, const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	vector<Matrix4> matrices(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		matrices[n] = m_nodes[n].trans;
		if(m_nodes[n].parent_id != -1)
			matrices[n] = matrices[m_nodes[n].parent_id] * matrices[n];
		for(int mesh_id : m_nodes[n].mesh_ids)
			m_meshes[mesh_id].draw(out, material, matrices[n]);
	}

	out.popViewMatrix();
}

void Mesh::printHierarchy() const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		printf("%d: %s\n", n, m_nodes[n].name.c_str());
}

SimpleMesh Mesh::toSimpleMesh() const {
	vector<Matrix4> matrices(m_nodes.size());
	vector<SimpleMesh> meshes;

	for(int n = 0; n < (int)m_nodes.size(); n++) {
		matrices[n] = m_nodes[n].trans;
		if(m_nodes[n].parent_id != -1)
			matrices[n] = matrices[m_nodes[n].parent_id] * matrices[n];

		for(int mesh_id : m_nodes[n].mesh_ids)
			meshes.emplace_back(SimpleMesh::transform(matrices[n], m_meshes[mesh_id]));
	}

	return SimpleMesh::merge(meshes);
}

float Mesh::intersect(const Segment &segment) const {
	float min_isect = constant::inf;

	vector<Matrix4> matrices(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		matrices[n] = m_nodes[n].trans;
		if(m_nodes[n].parent_id != -1)
			matrices[n] = matrices[m_nodes[n].parent_id] * matrices[n];

		for(int mesh_id : m_nodes[n].mesh_ids) {
			float isect = m_meshes[mesh_id].intersect(inverse(matrices[n]) * segment);
			min_isect = min(min_isect, isect);
		}
	}

	return min_isect;
}
}
