/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_profile.h"
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <map>
#include <algorithm>
#include <functional>
#include <numeric>

namespace fwk {

SkinnedMesh::MeshSkin::MeshSkin(const XMLNode &node) {
	using namespace xml_conversions;
	XMLNode counts_node = node.child("counts");
	XMLNode weights_node = node.child("weights");
	XMLNode joint_ids_node = node.child("joint_ids");
	if(!counts_node && !weights_node && !joint_ids_node)
		return;

	ASSERT(counts_node && weights_node && joint_ids_node);
	auto counts = fromString<vector<int>>(counts_node.value());
	auto weights = fromString<vector<float>>(weights_node.value());
	auto joint_ids = fromString<vector<int>>(joint_ids_node.value());

	ASSERT(weights.size() == joint_ids.size());
	ASSERT(std::accumulate(begin(counts), end(counts), 0) == (int)weights.size());

	vertex_weights.resize(counts.size());
	int offset = 0;
	for(int n = 0; n < (int)counts.size(); n++) {
		auto &out = vertex_weights[n];
		out.resize(counts[n]);
		for(int i = 0; i < counts[n]; i++)
			out[i] = VertexWeight(weights[offset + i], joint_ids[offset + i]);
		offset += counts[n];
	}
}

void SkinnedMesh::MeshSkin::saveToXML(XMLNode node) const {
	if(isEmpty())
		return;

	vector<int> counts;
	vector<float> weights;
	vector<int> joint_ids;

	for(int n = 0; n < (int)vertex_weights.size(); n++) {
		const auto &in = vertex_weights[n];
		counts.emplace_back((int)in.size());
		for(int i = 0; i < (int)in.size(); i++) {
			weights.emplace_back(in[i].weight);
			joint_ids.emplace_back(in[i].joint_id);
		}
	}

	using namespace xml_conversions;
	node.addChild("counts", node.own(toString(counts)));
	node.addChild("weights", node.own(toString(weights)));
	node.addChild("joint_ids", node.own(toString(joint_ids)));
}

bool SkinnedMesh::MeshSkin::isEmpty() const {
	return std::all_of(begin(vertex_weights), end(vertex_weights),
					   [](const auto &v) { return v.empty(); });
}

SkinnedMesh::SkinnedMesh() : m_bind_scale(1, 1, 1) {}

SkinnedMesh::SkinnedMesh(const aiScene &ascene) : Mesh(ascene) {
	vector<Matrix4> mats(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		mats[n] = m_nodes[n].trans;
		if(m_nodes[n].parent_id != -1)
			mats[n] = mats[m_nodes[n].parent_id] * mats[n];
	}

	m_inv_bind_matrices.resize(m_nodes.size(), Matrix4::identity());
	m_mesh_skins.resize(m_meshes.size());

	for(uint m = 0; m < ascene.mNumMeshes; m++) {
		const aiMesh &amesh = *ascene.mMeshes[m];
		ASSERT((int)amesh.mNumVertices == m_meshes[m].vertexCount());
		MeshSkin &skin = m_mesh_skins[m];
		skin.vertex_weights.resize(amesh.mNumVertices);

		for(uint n = 0; n < amesh.mNumBones; n++) {
			const aiBone &abone = *amesh.mBones[n];
			int joint_id = findNode(abone.mName.C_Str());
			ASSERT(joint_id != -1);
			const auto &joint = m_nodes[joint_id];

			m_inv_bind_matrices[joint_id] = transpose(Matrix4(abone.mOffsetMatrix[0]));
			for(uint w = 0; w < abone.mNumWeights; w++) {
				const aiVertexWeight &aweight = abone.mWeights[w];
				skin.vertex_weights[aweight.mVertexId].emplace_back(aweight.mWeight, joint_id);
			}
		}
	}

	m_bind_scale = float3(1, 1, 1);
	m_bind_matrices.resize(m_nodes.size());
	for(int n = 0; n < (int)m_inv_bind_matrices.size(); n++) {
		const Matrix4 &inv_bind = m_inv_bind_matrices[n];
		m_bind_matrices[n] = inverse(inv_bind);
		m_bind_scale =
			min(m_bind_scale, float3(length(inv_bind[0].xyz()), length(inv_bind[1].xyz()),
									 length(inv_bind[2].xyz())));
	}
	// TODO: fixing this value fixes junker on new assimp
	m_bind_scale = inv(m_bind_scale);
	filterSkinnedMeshes();
	verifyData();
}

SkinnedMesh::SkinnedMesh(const XMLNode &node) : Mesh(node) {
	m_mesh_skins.resize(m_meshes.size());
	XMLNode skin_node = node.child("skin");
	while(skin_node) {
		int mesh_id = skin_node.attrib<int>("mesh_id");
		ASSERT(mesh_id >= 0 && mesh_id < (int)m_mesh_skins.size());
		m_mesh_skins[mesh_id] = MeshSkin(skin_node);
		skin_node.next();
	}

	using namespace xml_conversions;
	if(const char *bind_scale_string = node.hasAttrib("bind_scale"))
		m_bind_scale = fromString<float3>(bind_scale_string);
	else
		m_bind_scale = float3(1, 1, 1);

	XMLNode bind_matrices_node = node.child("bind_matrices");
	if(bind_matrices_node) {
		m_bind_matrices = fromString<vector<Matrix4>>(bind_matrices_node.value());
		m_inv_bind_matrices.reserve(m_bind_matrices.size());
		for(const auto &matrix : m_bind_matrices)
			m_inv_bind_matrices.emplace_back(inverse(matrix));
	} else {
		m_bind_matrices = vector<Matrix4>(m_nodes.size(), Matrix4::identity());
		m_inv_bind_matrices = m_bind_matrices;
	}

	filterSkinnedMeshes();
	verifyData();
}

void SkinnedMesh::filterSkinnedMeshes() {
	vector<int> is_animated(m_meshes.size());

	m_skinned_mesh_ids.clear();
	for(int n = 0; n < (int)m_mesh_skins.size(); n++) {
		is_animated[n] = !m_mesh_skins[n].isEmpty();
		if(is_animated[n])
			m_skinned_mesh_ids.emplace_back(n);
	}

	for(auto &node : m_nodes) {
		vector<int> mesh_ids;
		for(int n = 0; n < (int)node.mesh_ids.size(); n++)
			if(!is_animated[node.mesh_ids[n]])
				mesh_ids.emplace_back(node.mesh_ids[n]);
		node.mesh_ids = std::move(mesh_ids);
	}
}

void SkinnedMesh::verifyData() {
	ASSERT(m_bind_matrices.size() == m_nodes.size());
	ASSERT(m_mesh_skins.size() == m_meshes.size());
}

void SkinnedMesh::saveToXML(XMLNode node) const {
	Mesh::saveToXML(node);
	if(m_bind_scale != float3(1, 1, 1))
		node.addAttrib("bind_scale", m_bind_scale);

	for(int n = 0; n < (int)m_mesh_skins.size(); n++) {
		const auto &mesh_skin = m_mesh_skins[n];
		if(!mesh_skin.isEmpty()) {
			XMLNode skin_node = node.addChild("skin");
			skin_node.addAttrib("mesh_id", n);
			mesh_skin.saveToXML(skin_node);
		}
	}

	if(std::any_of(begin(m_bind_matrices), end(m_bind_matrices),
				   [](const auto &mat) { return mat != Matrix4::identity(); }))
		node.addChild("bind_matrices", node.own(xml_conversions::toString(m_bind_matrices)));
}

aiScene *SkinnedMesh::toAIScene() const {
	aiScene *out = Mesh::toAIScene();
	auto *root_node = out->mRootNode;
	ASSERT(root_node);

	delete[] root_node->mMeshes;
	vector<int> mesh_ids = m_nodes.front().mesh_ids;
	mesh_ids.insert(begin(mesh_ids), begin(m_skinned_mesh_ids), end(m_skinned_mesh_ids));

	root_node->mNumMeshes = (int)mesh_ids.size();
	root_node->mMeshes = new unsigned int[root_node->mNumMeshes];
	std::copy(begin(mesh_ids), end(mesh_ids), root_node->mMeshes);

	return out;
}

void SkinnedMesh::drawSkeleton(Renderer &out, const MeshPose &pose, Color color) const {
	SimpleMesh bbox_mesh(MakeBBox(), FBox(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f));
	out.pushViewMatrix();

	Matrix4 matrix = Matrix4::identity();

	const auto &final_pose = finalPose(pose);
	vector<float3> positions(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		positions[n] = mulPoint(final_pose[n], m_bind_matrices[n][3].xyz());
		positions[n] = mulPoint(matrix, positions[n]);
	}

	for(int n = 0; n < (int)m_nodes.size(); n++)
		if(m_inv_bind_matrices[n] != Matrix4::identity())
			bbox_mesh.draw(out, {Color::green}, translation(positions[n]));

	out.popViewMatrix();
}

FBox SkinnedMesh::boundingBox(const MeshPose &pose) const {
	FBox out = Mesh::boundingBox(pose);

	for(int mesh_id : m_skinned_mesh_ids) {
		const auto &mesh = m_meshes[mesh_id];
		PodArray<float3> positions(mesh.vertexCount());
		animateVertices(mesh_id, pose, positions.data(), nullptr);
		FBox bbox(positions);
		out = out.isEmpty() ? bbox : sum(out, bbox);
	}

	return out;
}

float SkinnedMesh::intersect(const Segment &segment, const MeshPose &pose) const {
	float min_isect = Mesh::intersect(segment, pose);

	for(int mesh_id : m_skinned_mesh_ids) {
		const auto &mesh = m_meshes[mesh_id];
		PodArray<float3> positions(mesh.vertexCount());
		animateVertices(mesh_id, pose, positions.data(), nullptr);

		// TODO: optimize
		if(intersection(segment, FBox(positions)) < constant::inf)
			for(const auto &tri : mesh.trisIndices()) {
				float isect = intersection(
					segment, Triangle(positions[tri[0]], positions[tri[1]], positions[tri[2]]));
				min_isect = min(min_isect, isect);
			}
	}

	return min_isect;
}

void SkinnedMesh::animateVertices(int mesh_id, const MeshPose &pose, float3 *out_positions,
								  float3 *out_normals) const {
	DASSERT(mesh_id >= 0 && mesh_id < (int)m_meshes.size());

	const MeshSkin &skin = m_mesh_skins[mesh_id];
	const SimpleMesh &mesh = m_meshes[mesh_id];
	updateCounter("SM::animateVertices", 1);
	FWK_PROFILE("SM::animateVertices");
	DASSERT(!skin.isEmpty());

	const auto &final_pose = finalPose(pose);
	for(int v = 0; v < (int)skin.vertex_weights.size(); v++) {
		const auto &vweights = skin.vertex_weights[v];

		if(out_positions) {
			float3 pos = mesh.positions()[v];
			float3 out;
			for(const auto &weight : vweights) {
				ASSERT(weight.joint_id >= 0 && weight.joint_id < (int)pose.size());
				ASSERT(weight.weight >= 0 && weight.weight <= 1.0f);

				out += mulPointAffine(final_pose[weight.joint_id], pos) * weight.weight;
			}
			out_positions[v] = out;
		}
		if(out_normals) {
			float3 nrm = mesh.normals()[v];
			float3 out;
			for(const auto &weight : vweights)
				out += mulNormalAffine(final_pose[weight.joint_id], nrm) * weight.weight;
			out_normals[v] = out;
		}
	}
}

SimpleMesh SkinnedMesh::animateMesh(int mesh_id, const MeshPose &pose) const {
	vector<float3> positions = m_meshes[mesh_id].positions();
	vector<float2> tex_coords = m_meshes[mesh_id].texCoords();
	animateVertices(mesh_id, pose, positions.data(), nullptr);
	return SimpleMesh(positions, {}, tex_coords, m_meshes[mesh_id].indices());
}

void SkinnedMesh::draw(Renderer &out, const MeshPose &pose, const Material &material,
					   const Matrix4 &matrix) const {
	Mesh::draw(out, pose, material, matrix);

	out.pushViewMatrix();
	out.mulViewMatrix(matrix);
	for(int mesh_id : m_skinned_mesh_ids)
		SimpleMesh(animateMesh(mesh_id, pose)).draw(out, material, Matrix4::identity());
	// m_data.drawSkeleton(out, anim_id, anim_pos, material.color());
	out.popViewMatrix();
}

Matrix4 SkinnedMesh::nodeTrans(const string &name, const MeshPose &pose) const {
	const auto &final_pose = finalPose(pose);
	for(int n = 0; n < (int)m_nodes.size(); n++)
		if(m_nodes[n].name == name)
			return final_pose[n];
	return Matrix4::identity();
}

void SkinnedMesh::printHierarchy() const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		printf("%d: %s\n", n, m_nodes[n].name.c_str());
}

const vector<Matrix4> &SkinnedMesh::finalPose(const MeshPose &pose) const {
	DASSERT(pose.size() == m_nodes.size());
	if(pose.m_is_skinned_dirty) {
		const auto &normal_poses = Mesh::finalPose(pose);
		DASSERT(normal_poses.size() == m_nodes.size());

		auto &out = pose.m_skinned_final;
		out.resize(m_nodes.size());
		for(int n = 0; n < (int)out.size(); n++)
			out[n] = scaling(m_bind_scale) * normal_poses[n] * m_inv_bind_matrices[n];
		pose.m_is_skinned_dirty = false;
	}
	return pose.m_skinned_final;
}
}
