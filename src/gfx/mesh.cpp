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

namespace {

	void transToXML(const AffineTrans &trans, XMLNode node) {
		if(!areSimilar(trans.translation, float3()))
			node.addAttrib("pos", trans.translation);
		if(!areSimilar(trans.scale, float3(1, 1, 1)))
			node.addAttrib("scale", trans.scale);
		if(!areSimilar((float4)trans.rotation, (float4)Quat()))
			node.addAttrib("rot", trans.rotation);
	}

	AffineTrans transFromXML(XMLNode node) {
		using namespace xml_conversions;
		float3 pos, scale(1, 1, 1);
		Quat rot;

		if(auto *pos_string = node.hasAttrib("pos"))
			pos = fromString<float3>(pos_string);
		if(auto *scale_string = node.hasAttrib("scale"))
			scale = fromString<float3>(scale_string);
		if(auto *rot_string = node.hasAttrib("rot"))
			rot = fromString<Quat>(rot_string);
		return AffineTrans(pos, scale, rot);
	}
}

Mesh::Mesh(const aiScene &ascene) {
	auto *root_node = ascene.mRootNode;

	// Assimp is adding redundant node
	while(true) {
		auto name = root_node->mName;
		bool not_used = root_node->mNumChildren == 1 && root_node->mNumMeshes == 0 &&
						Matrix4(root_node->mTransformation[0]) == Matrix4::identity();

		for(int m = 0; m < (int)ascene.mNumMeshes; m++) {
			auto *mesh = ascene.mMeshes[m];
			for(int b = 0; b < (int)mesh->mNumBones; b++)
				if(mesh->mBones[b]->mName == name)
					not_used = false;
		}

		for(int a = 0; a < (int)ascene.mNumAnimations; a++) {
			auto *anim = ascene.mAnimations[a];
			for(int c = 0; c < (int)anim->mNumChannels; c++)
				if(anim->mChannels[c]->mNodeName == name)
					not_used = false;
		}

		if(not_used)
			root_node = root_node->mChildren[0];
		else
			break;
	}
	Matrix4 mat(root_node->mTransformation[0]);

	m_meshes.reserve(ascene.mNumMeshes);
	for(uint n = 0; n < ascene.mNumMeshes; n++)
		m_meshes.emplace_back(SimpleMesh(ascene, n));

	std::deque<pair<const aiNode *, int>> queue({make_pair(root_node, -1)});

	while(!queue.empty()) {
		auto *anode = queue.front().first;
		int parent_id = queue.front().second;
		queue.pop_front();

		Node new_node;
		new_node.name = anode->mName.C_Str();

		new_node.trans = transpose(Matrix4(anode->mTransformation[0]));
		new_node.parent_id = parent_id;

		for(uint m = 0; m < anode->mNumMeshes; m++)
			new_node.mesh_ids.emplace_back(anode->mMeshes[m]);

		parent_id = (int)m_nodes.size();
		m_nodes.emplace_back(new_node);

		for(uint c = 0; c < anode->mNumChildren; c++)
			queue.emplace_back(anode->mChildren[c], parent_id);
	}

	for(uint a = 0; a < ascene.mNumAnimations; a++)
		m_anims.emplace_back(ascene, (int)a, *this);

	verifyData();
}

Mesh::Mesh(const XMLNode &node) {
	XMLNode subnode = node.child("node");
	XMLNode mesh_node = node.child("simple_mesh");

	while(subnode) {
		m_nodes.emplace_back(Node{subnode.attrib("name"), transFromXML(subnode),
								  subnode.attrib<int>("parent_id"),
								  subnode.attrib<vector<int>>("mesh_ids")});
		subnode.next();
	}
	while(mesh_node) {
		m_meshes.emplace_back(mesh_node);
		mesh_node.next();
	}

	XMLNode anim_node = node.child("anim");
	while(anim_node) {
		m_anims.emplace_back(anim_node);
		anim_node.next();
	}

	verifyData();
}

void Mesh::verifyData() const {
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		const auto &node = m_nodes[n];
		ASSERT(node.parent_id >= -1 && node.parent_id < n);
		for(auto mesh_id : node.mesh_ids)
			ASSERT(mesh_id >= 0 && mesh_id < (int)m_meshes.size());
	}
}

void Mesh::saveToXML(XMLNode xml_node) const {
	for(const auto &node : m_nodes) {
		XMLNode subnode = xml_node.addChild("node");
		subnode.addAttrib("name", subnode.own(node.name));
		subnode.addAttrib("parent_id", node.parent_id);
		subnode.addAttrib("mesh_ids", node.mesh_ids);
		transToXML(node.trans, subnode);
	}

	for(const auto &mesh : m_meshes) {
		XMLNode mesh_node = xml_node.addChild("simple_mesh");
		mesh.saveToXML(mesh_node);
	}
	for(const auto &anim : m_anims)
		anim.saveToXML(xml_node.addChild("anim"));
}

aiScene *Mesh::toAIScene() const {
	// TODO: UGH: fix this...
	aiScene *out = static_cast<aiScene *>(malloc(sizeof(aiScene)));
	memset(out, 0, sizeof(aiScene));

	vector<aiNode *> anodes;
	for(const auto &node : m_nodes) {
		aiNode *new_node = new aiNode(node.name.c_str());
		new_node->mNumMeshes = node.mesh_ids.size();
		new_node->mMeshes = new unsigned int[new_node->mNumMeshes];
		std::copy(begin(node.mesh_ids), end(node.mesh_ids), new_node->mMeshes);
		auto *parent = node.parent_id == -1 ? nullptr : anodes[node.parent_id];
		new_node->mParent = parent;
		if(parent)
			parent->mNumChildren++;
		Matrix4 mat = transpose(node.trans);
		new_node->mTransformation = aiMatrix4x4(
			mat[0][0], mat[0][1], mat[0][2], mat[0][3], mat[1][0], mat[1][1], mat[1][2], mat[1][3],
			mat[2][0], mat[2][1], mat[2][2], mat[2][3], mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
		anodes.emplace_back(new_node);
	}
	out->mRootNode = anodes.empty() ? nullptr : anodes[0];
	ASSERT(out->mRootNode);

	for(int p = 0; p < (int)anodes.size(); p++) {
		auto *parent = anodes[p];
		parent->mChildren = parent->mNumChildren ? new aiNode *[parent->mNumChildren] : nullptr;
		int child_id = 0;
		for(int c = p + 1; c < (int)anodes.size(); c++)
			if(anodes[c]->mParent == parent)
				parent->mChildren[child_id++] = anodes[c];
	}

	aiScene &scene = *out;
	scene.mMaterials = new aiMaterial *[1];
	scene.mNumMaterials = 1;
	scene.mMaterials[0] = new aiMaterial();

	scene.mMeshes = new aiMesh *[m_meshes.size()];
	scene.mNumMeshes = (int)m_meshes.size();
	for(int n = 0; n < (int)m_meshes.size(); n++)
		scene.mMeshes[n] = m_meshes[n].toAIMesh();

	scene.mNumAnimations = m_anims.size();
	scene.mAnimations = new aiAnimation *[m_anims.size()];
	for(int n = 0; n < (int)m_anims.size(); n++)
		scene.mAnimations[n] = m_anims[n].toAIAnimation();

	return out;
}

FBox Mesh::boundingBox(const MeshPose &pose) const {
	DASSERT(pose.size() == m_nodes.size());

	FBox out = FBox::empty();
	const auto &final_pose = finalPose(pose);
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		for(const auto &mesh : indexWith(m_meshes, m_nodes[n].mesh_ids)) {
			FBox bbox = final_pose[n] * mesh.boundingBox();
			out = out.isEmpty() ? bbox : sum(out, bbox);
		}
	}
	return out;
}

int Mesh::findNode(const string &name) const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		if(m_nodes[n].name == name)
			return n;
	return -1;
}

void Mesh::draw(Renderer &out, const MeshPose &pose, const Material &material,
				const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	const auto &final_pose = finalPose(pose);
	for(int n = 0; n < (int)m_nodes.size(); n++)
		for(int mesh_id : m_nodes[n].mesh_ids)
			m_meshes[mesh_id].draw(out, material, final_pose[n]);

	out.popViewMatrix();
}

void Mesh::printHierarchy() const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		printf("%d: %s\n", n, m_nodes[n].name.c_str());
}

SimpleMesh Mesh::toSimpleMesh(const MeshPose &pose) const {
	vector<SimpleMesh> meshes;

	const auto &final_pose = finalPose(pose);
	for(int n = 0; n < (int)m_nodes.size(); n++)
		for(const auto &mesh : indexWith(m_meshes, m_nodes[n].mesh_ids))
			meshes.emplace_back(SimpleMesh::transform(final_pose[n], mesh));

	return SimpleMesh::merge(meshes);
}

float Mesh::intersect(const Segment &segment, const MeshPose &pose) const {
	float min_isect = constant::inf;

	const auto &final_pose = finalPose(pose);
	for(int n = 0; n < (int)m_nodes.size(); n++)
		for(const auto &mesh : indexWith(m_meshes, m_nodes[n].mesh_ids)) {
			float isect = mesh.intersect(inverse(final_pose[n]) * segment);
			min_isect = min(min_isect, isect);
		}

	return min_isect;
}

const vector<Matrix4> &Mesh::finalPose(const MeshPose &pose) const {
	DASSERT(pose.size() == m_nodes.size());
	if(pose.m_is_dirty) {
		auto &out = pose.m_final;
		out = std::vector<Matrix4>(begin(pose.m_transforms), end(pose.m_transforms));
		for(int n = 0; n < (int)out.size(); n++)
			if(m_nodes[n].parent_id != -1)
				out[n] = out[m_nodes[n].parent_id] * out[n];
		pose.m_is_dirty = false;
	}
	return pose.m_final;
}

MeshPose Mesh::defaultPose() const {
	vector<AffineTrans> out(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++)
		out[n] = m_nodes[n].trans;
	return MeshPose(std::move(out));
}

MeshPose Mesh::animatePose(int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < (int)m_anims.size());

	if(anim_id == -1)
		return defaultPose();
	return m_anims[anim_id].animatePose(defaultPose(), anim_pos);
}
}
