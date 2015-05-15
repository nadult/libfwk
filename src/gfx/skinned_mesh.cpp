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
	rot = Quat(Matrix3(mat[0].xyz(), mat[1].xyz(), mat[2].xyz()));
	rot = normalize(rot);
	pos = mat[3].xyz();
}

Skeleton::Trans::operator const Matrix4() const {
	const Matrix3 rot_matrix(rot);
	return Matrix4(float4(rot_matrix[0], 0.0f), float4(rot_matrix[1], 0.0f),
				   float4(rot_matrix[2], 0.0f), float4(pos, 1.0f));
}

Matrix4 lerp(const Skeleton::Trans &lhs, const Skeleton::Trans &rhs, float t) {
	Quat slerped = slerp(dot(lhs.rot, rhs.rot) < 0.0f ? -lhs.rot : lhs.rot, rhs.rot, t);
	Matrix3 rot_matrix(slerped);
	float3 pos = lerp(lhs.pos, rhs.pos, t);

	return Matrix4(float4(rot_matrix[0], 0.0f), float4(rot_matrix[1], 0.0f),
				   float4(rot_matrix[2], 0.0f), float4(pos, 1.0f));
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
	DASSERT(anim_id >= 0 && anim_id < ascene.mNumAnimations);

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
		new_channel.keys.resize(achannel.mNumPositionKeys);

		for(uint k = 0; k < achannel.mNumPositionKeys; k++) {
			ASSERT(achannel.mPositionKeys[k].mTime == achannel.mRotationKeys[k].mTime);
			new_channel.keys[k].trans = Trans(convert(achannel.mPositionKeys[k].mValue),
											  convert(achannel.mRotationKeys[k].mValue));
			new_channel.keys[k].time = achannel.mPositionKeys[k].mTime;
		}
		m_channels.push_back(std::move(new_channel));
	}

	m_name = aanim.mName.C_Str();
	m_length = aanim.mDuration;
}

void SkeletalAnim::animateJoints(Matrix4 *out, const Skeleton &skeleton, double anim_pos) const {
	if(anim_pos >= m_length)
		anim_pos -= double(int(anim_pos / m_length)) * m_length;

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

void SkinnedMeshData::drawSkeleton(Renderer &out, int anim_id, double anim_pos, Color color) const {
	SimpleMeshData mesh(MakeBBox(), FBox(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f));
	auto simple_mesh = make_shared<SimpleMesh>(mesh, color);
	out.pushViewMatrix();

	vector<Matrix4> joints(m_skeleton.size());
	animateJoints(joints.data(), anim_id, anim_pos);

	vector<float3> positions(m_skeleton.size());
	computeJointPositions(joints.data(), Matrix4::identity(), positions.data());

	for(int n = 0; n < (int)joints.size(); n++)
		if(m_inv_bind_matrices[n] != Matrix4::identity())
			simple_mesh->draw(out, {Color::green}, translation(positions[n]));

	out.popViewMatrix();
}
	
FBox SkinnedMeshData::computeBoundingBox(int anim_id, double anim_pos) const {
	PodArray<Matrix4> joints(m_skeleton.size());
	animateJoints(joints.data(), anim_id, anim_pos);

	FBox out = FBox::empty();
	for(int n = 0; n < (int)m_meshes.size(); n++) {
		PodArray<float3> positions(m_meshes[n].vertexCount());
		animateVertices(n, joints.data(), positions.data(), nullptr);
		FBox bbox(positions.data(), positions.size());
		out = n == 0? bbox : sum(m_bounding_box, bbox);
	}
	return out;
}

void SkinnedMeshData::computeBoundingBox() {
	//TODO: make this more accurate
	FBox bbox = computeBoundingBox(-1, 0.0f);
	for(int a = 0; a < animCount(); a++)
		bbox = sum(bbox, computeBoundingBox(a, 0.0f));
	m_bounding_box = FBox(bbox.center() - bbox.size(), bbox.center() + bbox.size());
}
	
float SkinnedMeshData::intersect(const Segment &segment, int anim_id, float anim_pos) const {
	PodArray<Matrix4> joints(m_skeleton.size());
	animateJoints(joints.data(), anim_id, anim_pos);

	for(int n = 0; n < (int)m_meshes.size(); n++) {
		PodArray<float3> positions(m_meshes[n].vertexCount());
		animateVertices(n, joints.data(), positions.data(), nullptr);

		float isect = intersection(segment, FBox(positions.data(), positions.size()));
		if(isect < constant::inf && isect >= segment.min && isect <= segment.max) {
			//TODO: intersect polygons
			return isect;
		}
	}

	return constant::inf;
}

void SkinnedMeshData::animateVertices(int mesh_id, const Matrix4 *joints, float3 *out_positions,
									  float3 *out_normals) const {
	DASSERT(mesh_id >= 0 && mesh_id < (int)m_meshes.size());
	const MeshSkin &skin = m_mesh_skins[mesh_id];
	const SimpleMeshData &mesh = m_meshes[mesh_id];

	for(int v = 0; v < (int)skin.vertex_weights.size(); v++) {
		const auto &vweights = skin.vertex_weights[v];

		if(out_positions) {
			float3 pos = mesh.positions()[v];
			float3 out;
			for(const auto &weight : vweights)
				out += mulPointAffine(joints[weight.joint_id], pos) * weight.weight;
			out_positions[v] = out;
		}
		if(out_normals) {
			float3 nrm = mesh.normals()[v];
			float3 out;
			for(const auto &weight : vweights)
				out += mulNormalAffine(joints[weight.joint_id], nrm) * weight.weight;
			out_normals[v] = out;
		}
	}
}

SimpleMeshData SkinnedMeshData::animateMesh(int mesh_id, const Matrix4 *joints) const {
	vector<float3> positions = m_meshes[mesh_id].positions();
	vector<float2> tex_coords = m_meshes[mesh_id].texCoords();
	animateVertices(mesh_id, joints, positions.data(), nullptr);
	return SimpleMeshData(positions, tex_coords, m_meshes[mesh_id].indices());
}

void SkinnedMeshData::computeJointPositions(const Matrix4 *joints, const Matrix4 &trans,
											float3 *out) const {
	for(int n = 0; n < m_skeleton.size(); n++) {
		out[n] = mulPoint(joints[n], m_bind_matrices[n][3].xyz());
		out[n] = mulPoint(trans, out[n]);
	}
}

void SkinnedMeshData::animateJoints(Matrix4 *trans, int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < (int)m_anims.size());

	if(anim_id == -1) {
		for(int n = 0; n < m_skeleton.size(); n++) {
			const auto &joint = m_skeleton[n];
			trans[n] = joint.trans;
			if(joint.parent_id != -1)
				trans[n] = trans[joint.parent_id] * trans[n];
		}
	} else { m_anims[anim_id].animateJoints(trans, m_skeleton, anim_pos); }

	for(int n = 0; n < m_skeleton.size(); n++)
		trans[n] = scaling(m_bind_scale) * trans[n] * m_inv_bind_matrices[n];
}

// TODO: problems when importing junker on custom compiled assimp (mesh is too small)
SkinnedMesh::SkinnedMesh(const aiScene &ascene) : m_data(ascene) {}

void SkinnedMesh::draw(Renderer &out, int anim_id, double anim_pos, const Material &material,
					   const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	vector<Matrix4> joints(m_data.skeleton().size());
	m_data.animateJoints(joints.data(), anim_id, anim_pos);
	// m_data.drawSkeleton(out, anim_id, anim_pos, material.color());

	for(int m = 0; m < (int)m_data.meshes().size(); m++)
		SimpleMesh(m_data.animateMesh(m, joints.data())).draw(out, material, Matrix4::identity());

	out.popViewMatrix();
}

}
