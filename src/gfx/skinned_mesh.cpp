/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <deque>
#include <map>

namespace fwk {

namespace {
	float3 convert(const aiVector3t<float> &vec) { return float3(vec.x, vec.y, vec.z); }
	Quat convert(const aiQuaterniont<float> &quat) {
		return Quat(float4(quat.x, quat.y, quat.z, quat.w));
	}
}

Skeleton::Trans::Trans(const Matrix4 &mat) {
	scale = float3(1, 1, 1);
	rot = Quat(Matrix3(mat[0].xyz(), mat[1].xyz(), mat[2].xyz()));
	rot = normalize(rot);
	pos = mat[3].xyz();
}

Skeleton::Trans::operator const Matrix4() const {
	const Matrix3 rot_matrix = Matrix3(rot) * scaling(scale);
	return Matrix4(float4(rot_matrix[0], 0.0f), float4(rot_matrix[1], 0.0f),
				   float4(rot_matrix[2], 0.0f), float4(pos, 1.0f));
}

Matrix4 lerp(const Skeleton::Trans &lhs, const Skeleton::Trans &rhs, float t) {
	// TODO: there is some problem with interpolation of attack animation in knight.dae
	Quat rot = slerp(lhs.rot, rhs.rot, t);
	float3 scale = lerp(lhs.scale, rhs.scale, t);
	float3 pos = lerp(lhs.pos, rhs.pos, t);
	return Matrix4(Skeleton::Trans(scale, pos, rot));
}

Skeleton::Skeleton(const aiScene &ascene) {
	auto *root_node = ascene.mRootNode;
	DASSERT(root_node);

	std::deque<pair<const aiNode *, int>> queue({make_pair(ascene.mRootNode, -1)});

	while(!queue.empty()) {
		auto *anode = queue.front().first;
		int parent_id = queue.front().second;
		queue.pop_front();

		int current_id = (int)m_joints.size();
		m_joints.emplace_back(
			Joint{transpose(Matrix4(anode->mTransformation[0])), anode->mName.C_Str(), parent_id});

		for(uint c = 0; c < anode->mNumChildren; c++)
			queue.emplace_back(anode->mChildren[c], current_id);
	}
}

int Skeleton::findJoint(const string &name) const {
	for(int n = 0; n < size(); n++)
		if(m_joints[n].name == name)
			return n;
	return -1;
}

SkeletalAnim::SkeletalAnim(const aiScene &ascene, int anim_id, const Skeleton &skeleton) {
	DASSERT(anim_id >= 0 && anim_id < (int)ascene.mNumAnimations);

	const auto &aanim = *ascene.mAnimations[anim_id];

	for(uint c = 0; c < aanim.mNumChannels; c++) {
		const auto &achannel = *aanim.mChannels[c];

		int joint_id = skeleton.findJoint(achannel.mNodeName.C_Str());
		if(joint_id == -1)
			continue;

		Channel new_channel;
		new_channel.joint_name = achannel.mNodeName.C_Str();
		new_channel.joint_id = joint_id;

		ASSERT(achannel.mNumRotationKeys == achannel.mNumPositionKeys);
		ASSERT(achannel.mNumScalingKeys == achannel.mNumPositionKeys);
		new_channel.keys.resize(achannel.mNumPositionKeys);

		for(uint k = 0; k < achannel.mNumPositionKeys; k++) {
			ASSERT(achannel.mPositionKeys[k].mTime == achannel.mRotationKeys[k].mTime);
			ASSERT(achannel.mScalingKeys[k].mTime == achannel.mRotationKeys[k].mTime);

			float3 scl = convert(achannel.mScalingKeys[k].mValue);
			//			if(scl != float3(1, 1, 1))
			//				printf("%f %f %f\n", scl.x, scl.y, scl.z);

			new_channel.keys[k].trans = Trans(convert(achannel.mScalingKeys[k].mValue),
											  convert(achannel.mPositionKeys[k].mValue),
											  convert(achannel.mRotationKeys[k].mValue));
			new_channel.keys[k].time = achannel.mPositionKeys[k].mTime;
		}
		m_channels.push_back(std::move(new_channel));
	}

	m_name = aanim.mName.C_Str();
	m_length = aanim.mDuration;
}

SkeletonPose SkeletalAnim::animateSkeleton(const Skeleton &skeleton, double anim_pos) const {
	if(anim_pos >= m_length)
		anim_pos -= double(int(anim_pos / m_length)) * m_length;

	SkeletonPose out(skeleton.size());
	for(int n = 0; n < skeleton.size(); n++)
		out[n] = skeleton[n].trans;

	for(int n = 0; n < (int)m_channels.size(); n++) {
		const Channel &channel = m_channels[n];
		DASSERT(channel.joint_id < skeleton.size() &&
				channel.joint_name == skeleton[channel.joint_id].name);

		int frame0 = 0, frame1 = 0, frame_count = (int)channel.keys.size();
		// TODO: binary search
		while(frame1 < frame_count && anim_pos > channel.keys[frame1].time) {
			frame0 = frame1;
			frame1++;
		}

		float diff = channel.keys[frame1].time - channel.keys[frame0].time;
		float blend_factor =
			diff < constant::epsilon ? 0.0f : (anim_pos - channel.keys[frame0].time) / diff;

		const Trans &trans0 = channel.keys[frame0].trans;
		const Trans &trans1 = channel.keys[frame1].trans;
		out[channel.joint_id] = lerp(trans0, trans1, blend_factor);
	}

	for(int n = 0; n < skeleton.size(); n++)
		if(skeleton[n].parent_id != -1)
			out[n] = out[skeleton[n].parent_id] * out[n];
	return out;
}

SkinnedMeshData::SkinnedMeshData() : m_bind_scale(1, 1, 1) {}

SkinnedMeshData::SkinnedMeshData(const aiScene &ascene) : MeshData(ascene), m_skeleton(ascene) {
	for(uint a = 0; a < ascene.mNumAnimations; a++)
		m_anims.emplace_back(ascene, (int)a, m_skeleton);

	vector<Matrix4> mats(m_skeleton.size());
	for(int n = 0; n < m_skeleton.size(); n++) {
		mats[n] = m_skeleton[n].trans;
		if(m_skeleton[n].parent_id != -1)
			mats[n] = mats[m_skeleton[n].parent_id] * mats[n];
	}

	m_inv_bind_matrices.resize(m_skeleton.size(), Matrix4::identity());
	m_mesh_skins.resize(m_meshes.size());

	for(uint m = 0; m < ascene.mNumMeshes; m++) {
		const aiMesh &amesh = *ascene.mMeshes[m];
		ASSERT((int)amesh.mNumVertices == m_meshes[m].vertexCount());
		MeshSkin &skin = m_mesh_skins[m];
		skin.vertex_weights.resize(amesh.mNumVertices);

		for(uint n = 0; n < amesh.mNumBones; n++) {
			const aiBone &abone = *amesh.mBones[n];
			int joint_id = m_skeleton.findJoint(abone.mName.C_Str());
			ASSERT(joint_id != -1);
			const auto &joint = m_skeleton[joint_id];

			m_inv_bind_matrices[joint_id] = transpose(Matrix4(abone.mOffsetMatrix[0]));
			for(uint w = 0; w < abone.mNumWeights; w++) {
				const aiVertexWeight &aweight = abone.mWeights[w];
				skin.vertex_weights[aweight.mVertexId].emplace_back(aweight.mWeight, joint_id);
			}
		}
	}

	m_bind_scale = float3(1, 1, 1);
	m_bind_matrices.resize(m_skeleton.size());
	for(int n = 0; n < (int)m_inv_bind_matrices.size(); n++) {
		const Matrix4 &inv_bind = m_inv_bind_matrices[n];
		m_bind_matrices[n] = inverse(inv_bind);
		m_bind_scale =
			min(m_bind_scale, float3(length(inv_bind[0].xyz()), length(inv_bind[1].xyz()),
									 length(inv_bind[2].xyz())));
	}
	// TODO: fixing this value fixes junker on new assimp
	m_bind_scale = inv(m_bind_scale);
	computeBoundingBox();
}

void SkinnedMeshData::drawSkeleton(Renderer &out, const SkeletonPose &pose, Color color) const {
	SimpleMeshData mesh(MakeBBox(), FBox(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f));
	auto simple_mesh = make_shared<SimpleMesh>(mesh, color);
	out.pushViewMatrix();

	Matrix4 matrix = Matrix4::identity();

	vector<float3> positions(m_skeleton.size());
	for(int n = 0; n < (int)m_skeleton.size(); n++) {
		positions[n] = mulPoint(pose[n], m_bind_matrices[n][3].xyz());
		positions[n] = mulPoint(matrix, positions[n]);
	}

	for(int n = 0; n < (int)m_skeleton.size(); n++)
		if(m_inv_bind_matrices[n] != Matrix4::identity())
			simple_mesh->draw(out, {Color::green}, translation(positions[n]));

	out.popViewMatrix();
}

FBox SkinnedMeshData::computeBoundingBox(const SkeletonPose &pose) const {
	FBox out = FBox::empty();

	for(int n = 0; n < (int)m_meshes.size(); n++) {
		PodArray<float3> positions(m_meshes[n].vertexCount());
		animateVertices(n, pose, positions.data(), nullptr);
		FBox bbox(positions);
		out = n == 0 ? bbox : sum(m_bounding_box, bbox);
	}

	return out;
}

void SkinnedMeshData::computeBoundingBox() {
	// TODO: make this more accurate
	FBox bbox = computeBoundingBox(animateSkeleton(-1, 0.0f));
	for(int a = 0; a < animCount(); a++)
		bbox = sum(bbox, computeBoundingBox(animateSkeleton(a, 0.0f)));
	m_bounding_box = FBox(bbox.center() - bbox.size(), bbox.center() + bbox.size());
}

float SkinnedMeshData::intersect(const Segment &segment, const SkeletonPose &pose) const {
	float min_isect = constant::inf;

	for(int n = 0; n < (int)m_meshes.size(); n++) {
		const auto &mesh = m_meshes[n];
		PodArray<float3> positions(mesh.vertexCount());
		animateVertices(n, pose, positions.data(), nullptr);

		if(intersection(segment, FBox(positions)) < constant::inf)
			for(const auto &tri : mesh.trisIndices()) {
				float isect =
					intersection(segment, positions[tri[0]], positions[tri[1]], positions[tri[2]]);
				min_isect = min(min_isect, isect);
			}
	}

	return min_isect;
}

void SkinnedMeshData::animateVertices(int mesh_id, const SkeletonPose &pose, float3 *out_positions,
									  float3 *out_normals) const {
	DASSERT(mesh_id >= 0 && mesh_id < (int)m_meshes.size());
	DASSERT((int)pose.size() == m_skeleton.size());
	const MeshSkin &skin = m_mesh_skins[mesh_id];
	const SimpleMeshData &mesh = m_meshes[mesh_id];

	for(int v = 0; v < (int)skin.vertex_weights.size(); v++) {
		const auto &vweights = skin.vertex_weights[v];

		if(out_positions) {
			float3 pos = mesh.positions()[v];
			float3 out;
			for(const auto &weight : vweights)
				out += mulPointAffine(pose[weight.joint_id], pos) * weight.weight;
			out_positions[v] = out;
		}
		if(out_normals) {
			float3 nrm = mesh.normals()[v];
			float3 out;
			for(const auto &weight : vweights)
				out += mulNormalAffine(pose[weight.joint_id], nrm) * weight.weight;
			out_normals[v] = out;
		}
	}
}

SimpleMeshData SkinnedMeshData::animateMesh(int mesh_id, const SkeletonPose &pose) const {
	DASSERT((int)pose.size() == m_skeleton.size());
	vector<float3> positions = m_meshes[mesh_id].positions();
	vector<float2> tex_coords = m_meshes[mesh_id].texCoords();
	animateVertices(mesh_id, pose, positions.data(), nullptr);
	return SimpleMeshData(positions, tex_coords, m_meshes[mesh_id].indices());
}

SkeletonPose SkinnedMeshData::animateSkeleton(int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < (int)m_anims.size());
	SkeletonPose out;

	if(anim_id == -1) {
		out.resize(m_skeleton.size());
		for(int n = 0; n < m_skeleton.size(); n++) {
			const auto &joint = m_skeleton[n];
			out[n] = joint.trans;
			if(joint.parent_id != -1)
				out[n] = out[joint.parent_id] * out[n];
		}
	} else { out = m_anims[anim_id].animateSkeleton(m_skeleton, anim_pos); }

	for(int n = 0; n < m_skeleton.size(); n++)
		out[n] = scaling(m_bind_scale) * out[n] * m_inv_bind_matrices[n];
	return out;
}

// TODO: problems when importing junker on custom compiled assimp (mesh is too small)
SkinnedMesh::SkinnedMesh(const aiScene &ascene) : m_data(ascene) {}

void SkinnedMesh::draw(Renderer &out, const SkeletonPose &pose, const Material &material,
					   const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);
	for(int m = 0; m < (int)m_data.meshes().size(); m++)
		SimpleMesh(m_data.animateMesh(m, pose)).draw(out, material, Matrix4::identity());
	out.popViewMatrix();
	// m_data.drawSkeleton(out, anim_id, anim_pos, material.color());
}

Matrix4 SkinnedMesh::nodeTrans(const string &name, const SkeletonPose &pose) const {
	for(int n = 0; n < (int)skeleton().size(); n++)
		if(m_data.skeleton()[n].name == name)
			return pose[n];
	return Matrix4::identity();
}

void SkinnedMesh::printHierarchy() const {
	for(int n = 0; n < m_data.skeleton().size(); n++)
		printf("%d: %s\n", n, m_data.skeleton()[n].name.c_str());
}
}
