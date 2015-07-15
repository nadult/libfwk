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

			for(uint w = 0; w < abone.mNumWeights; w++) {
				const aiVertexWeight &aweight = abone.mWeights[w];
				skin.vertex_weights[aweight.mVertexId].emplace_back(aweight.mWeight, joint_id);
			}
		}
	}

	computeBindMatrices();
	filterSkinnedMeshes();
	verifyData();
}

SkinnedMesh::SkinnedMesh(const XMLNode &node) : Mesh(node) {
	m_mesh_skins.resize(m_meshes.size());

	int mesh_id = 0;
	XMLNode mesh_node = node.child("simple_mesh");
	while(mesh_node) {
		if(XMLNode skin_node = mesh_node.child("skin"))
			m_mesh_skins[mesh_id] = MeshSkin(skin_node);
		mesh_node.next();
		mesh_id++;
	}

	using namespace xml_conversions;

	computeBindMatrices();
	filterSkinnedMeshes();
	verifyData();
}

void SkinnedMesh::computeBindMatrices() {
	auto def = defaultPose();

	m_bind_matrices = vector<Matrix4>(m_nodes.size(), Matrix4::identity());
	m_inv_bind_matrices.clear();

	for(int n = 0; n < (int)m_nodes.size(); n++) {
		Matrix4 mat = m_nodes[n].trans;
		if(m_nodes[n].parent_id != -1)
			mat = m_bind_matrices[m_nodes[n].parent_id] * mat;
		m_bind_matrices[n] = mat;
		m_inv_bind_matrices.emplace_back(inverse(mat));
	}

	m_bind_scale = float3(1, 1, 1);
	for(const auto &inv_bind : m_inv_bind_matrices) {
		m_bind_scale =
			min(m_bind_scale, float3(length(inv_bind[0].xyz()), length(inv_bind[1].xyz()),
									 length(inv_bind[2].xyz())));
	}
	// TODO: fixing this value fixes junker on new assimp
	m_bind_scale = inv(m_bind_scale);
}

Matrix4 SkinnedMesh::computeOffsetMatrix(int node_id) const {
	DASSERT(node_id >= 0 && node_id < (int)m_nodes.size());
	auto bind_pose = Mesh::finalPose(defaultPose());
	return bind_pose[node_id];
}

void SkinnedMesh::filterSkinnedMeshes() {
	m_skeleton_node_id = -1;

	for(auto &node : m_nodes) {
		if(node.type == MeshNodeType::skeleton)
			m_skeleton_node_id = node.id;

		vector<int> mesh_ids, skinned_ids;
		for(int n = 0; n < (int)node.mesh_ids.size(); n++) {
			int mesh_id = node.mesh_ids[n];
			(m_mesh_skins[mesh_id].isEmpty() ? mesh_ids : skinned_ids).emplace_back(mesh_id);
		}
		node.mesh_ids = std::move(mesh_ids);
		node.skinned_mesh_ids.insert(begin(node.skinned_mesh_ids), begin(skinned_ids),
									 end(skinned_ids));
	}
}

void SkinnedMesh::verifyData() {
	ASSERT(m_mesh_skins.size() == m_meshes.size());
}

void SkinnedMesh::saveToXML(XMLNode node) const {
	Mesh::saveToXML(node);

	XMLNode smesh_node = node.child("simple_mesh");
	int mesh_id = 0;
	while(smesh_node) {
		const auto &mesh_skin = m_mesh_skins[mesh_id];
		if(!mesh_skin.isEmpty())
			mesh_skin.saveToXML(smesh_node.addChild("skin"));
		smesh_node.next();
		mesh_id++;
	}
}

aiScene *SkinnedMesh::toAIScene() const {
	aiScene *out = Mesh::toAIScene();
	auto *root_node = out->mRootNode;
	ASSERT(root_node);

	delete[] root_node->mMeshes;
	vector<int> mesh_ids = m_nodes.front().mesh_ids;
	// TODO: fix or remove
	// mesh_ids.insert(begin(mesh_ids), begin(m_skinned_mesh_ids), end(m_skinned_mesh_ids));

	root_node->mNumMeshes = (int)mesh_ids.size();
	root_node->mMeshes = new unsigned int[root_node->mNumMeshes];
	std::copy(begin(mesh_ids), end(mesh_ids), root_node->mMeshes);

	return out;
}

void SkinnedMesh::drawSkeleton(Renderer &out, const MeshPose &pose, Color color,
							   const Matrix4 &matrix) const {
	SimpleMesh bbox_mesh(MakeBBox(), FBox(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f));
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	const auto &final_pose = finalPose(pose);
	vector<float3> positions(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++)
		positions[n] = mulPoint(final_pose[n], m_bind_matrices[n][3].xyz());

	for(int n = 0; n < (int)m_nodes.size(); n++) {
		if(m_inv_bind_matrices[n] != Matrix4::identity())
			bbox_mesh.draw(out, {Color::green}, translation(positions[n]));
		if(m_nodes[n].parent_id != -1) {
			vector<float3> lines{positions[n], positions[m_nodes[n].parent_id]};
			out.addLines(lines, color);
		}
	}

	out.popViewMatrix();
}

FBox SkinnedMesh::boundingBox(const MeshPose &pose) const {
	FBox out = Mesh::boundingBox(pose);

	for(int node_id = 0; node_id < (int)m_nodes.size(); node_id++)
		for(int mesh_id : m_nodes[node_id].skinned_mesh_ids) {
			const auto &mesh = m_meshes[mesh_id];
			PodArray<float3> positions(mesh.vertexCount());
			animateVertices(node_id, mesh_id, pose, positions.data(), nullptr);
			FBox bbox(positions);
			out = out.isEmpty() ? bbox : sum(out, bbox);
		}

	return out;
}

float SkinnedMesh::intersect(const Segment &segment, const MeshPose &pose) const {
	float min_isect = Mesh::intersect(segment, pose);

	for(int node_id = 0; node_id < (int)m_nodes.size(); node_id++)
		for(int mesh_id : m_nodes[node_id].skinned_mesh_ids) {
			const auto &mesh = m_meshes[mesh_id];
			PodArray<float3> positions(mesh.vertexCount());
			animateVertices(node_id, mesh_id, pose, positions.data(), nullptr);

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

void SkinnedMesh::animateVertices(int node_id, int mesh_id, const MeshPose &pose,
								  float3 *out_positions, float3 *out_normals) const {
	DASSERT(node_id >= 0 && node_id < (int)m_nodes.size());
	DASSERT(mesh_id >= 0 && mesh_id < (int)m_meshes.size());

	const auto &node = m_nodes[node_id];
	const MeshSkin &skin = m_mesh_skins[mesh_id];
	const SimpleMesh &mesh = m_meshes[mesh_id];
	updateCounter("SM::animateVertices", 1);
	FWK_PROFILE("SM::animateVertices");
	DASSERT(!skin.isEmpty());

	vector<Matrix4> matrices = finalPose(pose);
	Matrix4 offset_matrix = computeOffsetMatrix(node_id);

	for(auto &matrix : matrices)
		matrix = matrix * offset_matrix;

	for(int v = 0; v < (int)skin.vertex_weights.size(); v++) {
		const auto &vweights = skin.vertex_weights[v];

		if(out_positions) {
			float3 pos = mesh.positions()[v];
			float3 out;
			for(const auto &weight : vweights) {
				ASSERT(weight.joint_id >= 0 && weight.joint_id < (int)pose.size());
				ASSERT(weight.weight >= 0 && weight.weight <= 1.0f);

				out += mulPointAffine(matrices[weight.joint_id], pos) * weight.weight;
			}
			out_positions[v] = out;
		}
		if(out_normals) {
			float3 nrm = mesh.normals()[v];
			float3 out;
			for(const auto &weight : vweights)
				out += mulNormalAffine(matrices[weight.joint_id], nrm) * weight.weight;
			out_normals[v] = out;
		}
	}
}

SimpleMesh SkinnedMesh::animateMesh(int node_id, int mesh_id, const MeshPose &pose) const {
	vector<float3> positions = m_meshes[mesh_id].positions();
	vector<float2> tex_coords = m_meshes[mesh_id].texCoords();
	animateVertices(node_id, mesh_id, pose, positions.data(), nullptr);
	return SimpleMesh(positions, {}, tex_coords, m_meshes[mesh_id].indices());
}

void SkinnedMesh::draw(Renderer &out, const MeshPose &pose, const Material &material,
					   const Matrix4 &matrix) const {
	Mesh::draw(out, pose, material, matrix);

	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	for(int node_id = 0; node_id < (int)m_nodes.size(); node_id++)
		for(int mesh_id : m_nodes[node_id].skinned_mesh_ids)
			SimpleMesh(animateMesh(node_id, mesh_id, pose))
				.draw(out, material, Matrix4::identity());
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
