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

Skeleton::Skeleton(const aiScene &ascene, int mesh_id) {
	DASSERT(mesh_id >= 0 && mesh_id < ascene.mNumMeshes);
	const aiMesh &amesh = *ascene.mMeshes[mesh_id];

	if(amesh.HasBones()) {
		m_joints.reserve(amesh.mNumBones);

		auto *root_node = ascene.mRootNode;
		DASSERT(root_node);

		std::deque<pair<const aiNode *, int>> queue({make_pair(ascene.mRootNode, -1)});
		std::map<string, int> name_map;

		while(!queue.empty()) {
			auto *anode = queue.front().first;
			int parent_id = queue.front().second;
			queue.pop_front();

			Joint new_joint;
			new_joint.name = anode->mName.C_Str();

			new_joint.trans = transpose(Matrix4(anode->mTransformation[0]));
			new_joint.parent_id = parent_id;
			new_joint.inv_bind = Matrix4::identity();
			new_joint.is_dummy = true;

			parent_id = (int)m_joints.size();
			name_map[new_joint.name] = parent_id;
			m_joints.emplace_back(new_joint);

			for(uint c = 0; c < anode->mNumChildren; c++)
				queue.emplace_back(anode->mChildren[c], parent_id);
		}

		m_vertex_weights.resize(amesh.mNumVertices);

		for(uint n = 0; n < amesh.mNumBones; n++) {
			const aiBone &abone = *amesh.mBones[n];
			auto it = name_map.find(string(abone.mName.C_Str()));
			ASSERT(it != name_map.end());
			Joint &joint = m_joints[it->second];

			joint.inv_bind = transpose(Matrix4(abone.mOffsetMatrix[0]));
			joint.is_dummy = false;

			for(uint w = 0; w < abone.mNumWeights; w++) {
				const aiVertexWeight &aweight = abone.mWeights[w];
				m_vertex_weights[aweight.mVertexId].emplace_back(aweight.mWeight, it->second);
			}
		}
	}
}

int Skeleton::findJoint(const string &name) const {
	for(int n = 0; n < size(); n++)
		if(m_joints[n].name == name)
			return n;
	return -1;
}

vector<Matrix4> Skeleton::invBinds() const {
	vector<Matrix4> out(m_joints.size());
	for(int n = 0; n < size(); n++) {
		out[n] = m_joints[n].inv_bind;
	}
	return out;
}

SkeletalAnim::SkeletalAnim(const aiScene &ascene, int mesh_id, int anim_id,
						   const Skeleton &skeleton) {
	DASSERT(mesh_id >= 0 && mesh_id < ascene.mNumMeshes);
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

void SkeletalAnim::animate(Matrix4 *out, const Skeleton &skeleton, double anim_time) const {
	if(anim_time >= m_length)
		anim_time -= double(int(anim_time / m_length)) * m_length;

	for(int n = 0; n < skeleton.size(); n++)
		out[n] = skeleton.joints()[n].trans;

	for(int n = 0; n < (int)m_channels.size(); n++) {
		const Channel &channel = m_channels[n];
		DASSERT(channel.joint_id < skeleton.size() &&
				channel.joint_name == skeleton.joints()[channel.joint_id].name);

		int frame0 = 0, frame1 = 0, frame_count = (int)channel.keys.size();
		// TODO: binary search
		while(frame1 < frame_count && anim_time > channel.keys[frame1].time) {
			frame0 = frame1;
			frame1++;
		}

		float diff = channel.keys[frame1].time - channel.keys[frame0].time;
		float blend_factor =
			diff < constant::epsilon ? 0.0f : (anim_time - channel.keys[frame0].time) / diff;

		const Trans &trans0 = channel.keys[frame0].trans;
		const Trans &trans1 = channel.keys[frame1].trans;
		out[channel.joint_id] = lerp(trans0, trans1, blend_factor);
	}

	for(int n = 0; n < skeleton.size(); n++)
		if(skeleton.joints()[n].parent_id != -1)
			out[n] = out[skeleton.joints()[n].parent_id] * out[n];
}

SkinnedMeshData::SkinnedMeshData() : m_bind_scale(1, 1, 1) {}

SkinnedMeshData::SkinnedMeshData(const aiScene &ascene, int mesh_id)
	: SimpleMeshData(ascene, mesh_id), m_skeleton(ascene, mesh_id) {
	DASSERT(mesh_id >= 0 && mesh_id < ascene.mNumMeshes);

	for(uint a = 0; a < ascene.mNumAnimations; a++)
		m_anims.emplace_back(ascene, mesh_id, (int)a, m_skeleton);

	vector<Matrix4> mats(m_skeleton.size());
	for(int n = 0; n < m_skeleton.size(); n++) {
		mats[n] = m_skeleton.joints()[n].trans;
		if(m_skeleton.joints()[n].parent_id != -1)
			mats[n] = mats[m_skeleton.joints()[n].parent_id] * mats[n];
	}

	m_bind_scale = float3(1, 1, 1);
	m_inv_binds.resize(m_skeleton.size());
	m_binds.resize(m_skeleton.size());
	for(int n = 0; n < (int)m_skeleton.size(); n++) {
		const Matrix4 &inv_bind = m_skeleton.joints()[n].inv_bind;
		m_inv_binds[n] = inv_bind;
		m_binds[n] = inverse(inv_bind);
		m_bind_scale =
			min(m_bind_scale, float3(length(inv_bind[0].xyz()), length(inv_bind[1].xyz()),
									 length(inv_bind[2].xyz())));
	}
	m_bind_scale = inv(m_bind_scale);
}

const float3 SkinnedMeshData::transformPoint(int vertex_id, const Matrix4 *joints) const {
	float3 out(0, 0, 0);
	for(const auto &weight : m_skeleton.vertexWeights()[vertex_id])
		out += mulPoint(joints[weight.joint_id], m_positions[vertex_id]) * weight.weight;
	// TODO: xzy???
	return float3(out.x, out.z, out.y);
}

const float3 SkinnedMeshData::rootPos(int anim_id, double anim_time) const {
	// TODO: speed up
	Matrix4 joints[Skeleton::max_joints];
	animate(joints, anim_id, anim_time);
	return mulPoint(joints[0], m_binds[0][3].xyz());
}

void SkinnedMeshData::drawSkeleton(Renderer &out, int anim_id, float anim_pos, Color color) const {
	SimpleMeshData mesh(MakeBBox(), FBox(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f));
	auto simple_mesh = make_shared<SimpleMesh>(mesh, color);
	out.pushViewMatrix();

	vector<Matrix4> joints(m_skeleton.size());
	animate(joints.data(), anim_id, anim_pos);

	float3 positions[Skeleton::max_joints];
	computeJointPositions(joints.data(), Matrix4::identity(), positions);

	for(int n = 0; n < (int)joints.size(); n++)
		if(!m_skeleton.joints()[n].is_dummy)
			simple_mesh->draw(out, {Color::green}, translation(positions[n]));

	out.popViewMatrix();

	/*	float3 positions[Skeleton::max_joints];
		computeJointPositions(joints, trans, positions);

		for(int n = 0; n < m_skeleton.size(); n++) {
			out.addBox(FBox(-1, -1, -1, 1, 1, 1) * 0.1f, translation(positions[n]), true,
	   color);
			if(m_skeleton[n].parent_id != -1)
				out.addLine(positions[n], positions[m_skeleton[n].parent_id], color);
		}*/
}

void SkinnedMeshData::alignPoses(const Matrix4 *joints1, const Matrix4 *joints2, const Matrix4 &tr1,
								 const Matrix4 &tr2, float &out_error, float3 &out_offset) const {
	float3 pose1[Skeleton::max_joints], pose2[Skeleton::max_joints];
	computeJointPositions(joints1, tr1, pose1);
	computeJointPositions(joints2, tr2, pose2);

	float3 offset = pose2[0] - pose1[0];

	double error = 0.0;
	for(int n = 0; n < m_skeleton.size(); n++)
		error += lengthSq(pose2[n] - pose1[n] - offset);

	out_error = sqrt(error);
	out_offset = offset;
}

void SkinnedMeshData::alignAnim(const Matrix4 *pose, const Matrix4 &pose_trans, int anim_id,
								float &out_pos, float3 &out_offset) const {
	const SkeletalAnim &anim = m_anims[anim_id];

	float min_error = constant::inf, min_pos = 0.0f;
	float3 min_offset;

	/*
	for(int n = 0; n < anim.frameCount(); n++) {
		Matrix4 anim_pose[Skeleton::max_joints];
		animate(anim_pose, anim_id, anim.frameTime(n));
		float error;
		float3 offset;

		alignPoses(pose, anim_pose, pose_trans, Matrix4::identity(), error, offset);
		//	printf("Frame %d: err:%f offset: ", n, error);
		//	print(offset);
		//	printf("\n");

		if(error < min_error) {
			min_error = error;
			min_offset = offset;
			min_pos = anim.frameTime(n);
		}
	}*/

	out_pos = min_pos;
	out_offset = min_offset;
	//	printf("Best: pos:%f off:(%.2f %.2f %.2f) err:%f\n",
	//			out_pos, out_offset.x, out_offset.y, out_offset.z, min_error);
}

void SkinnedMeshData::computeBBox() {
	/*	PodArray<float3> temp(vertexCount());

		for(int a = 0; a < (int)m_anims.size(); a++) {
			const SkeletalAnim &anim = m_anims[a];
			int frame_count = anim.frameCount();
			for(int f = 0; f < frame_count; f++) {
				Matrix4 joints[Skeleton::max_joints];

				anim.animate(joints, m_skeleton, double(f) * anim.length() / double(frame_count
	   -
	   1));
				for(int j = 0; j < m_skeleton.size(); j++)
					joints[j] = scaling(m_bind_scale) * joints[j] * m_inv_bind[j];

				animateVertices(joints, temp.data());
				FBox bbox(temp.data(), temp.size());

				if(a == 0 && f == 0)
					m_bbox = bbox;
				else
					m_bbox = FBox(min(m_bbox.min, bbox.min), max(m_bbox.max, bbox.max));
			}
		}*/
}

void SkinnedMeshData::animateVertices(const Matrix4 *joints, float3 *out_positions,
									  float3 *out_normals) const {
	Matrix4 temp_joints[Skeleton::max_joints];
	if(!joints) {
		animate(temp_joints, 0, 0.0);
		joints = temp_joints;
	}

	const auto &vertex_weights = m_skeleton.vertexWeights();

	for(int v = 0; v < (int)vertex_weights.size(); v++) {
		const auto &vweights = vertex_weights[v];
		const float3 &pos = m_positions[v];
		const float3 &nrm = m_normals[v];

		if(out_positions) {
			float3 out(0, 0, 0);
			for(const auto &weight : vweights)
				out += mulPointAffine(joints[weight.joint_id], pos) * weight.weight;
			out_positions[v] = out;
		}
		if(out_normals) {
			float3 out(0, 0, 0);
			for(const auto &weight : vweights)
				out += mulNormalAffine(joints[weight.joint_id], nrm) * weight.weight;
			out_normals[v] = out;
		}
	}
}

void SkinnedMeshData::computeJointPositions(const Matrix4 *joints, const Matrix4 &trans,
											float3 *out) const {
	for(int n = 0; n < m_skeleton.size(); n++) {
		out[n] = mulPoint(joints[n], m_binds[n][3].xyz());
		out[n] = mulPoint(trans, out[n]);
	}
}

void SkinnedMeshData::animate(Matrix4 *trans, int anim_id, double anim_time) const {
	DASSERT(anim_id >= 0 && anim_id < (int)m_anims.size());
	m_anims[anim_id].animate(trans, m_skeleton, anim_time);

	//		printf("\n");
	//		for(int n = 0; n < m_skeleton.size(); n++) {
	//			float3 p = mulPoint(trans[n], float3(0, 0, 0));
	//			printf("%s: %f %f %f\n", m_skeleton[n].name.c_str(), p.x, p.y, p.z);
	//		}
	for(int n = 0; n < m_skeleton.size(); n++)
		trans[n] = scaling(m_bind_scale) * trans[n] * m_inv_binds[n];

	/*
	if(m_anims[anim_id].name() == "anim_standing_rifle_aim" ||
	   m_anims[anim_id].name() == "anim_standing_rifle_fire")
		for(int n = 0; n < m_skeleton.size(); n++)
			trans[n] = rotation(float3(0, 1, 0), constant::pi) * trans[n];*/
}

SkinnedMesh::SkinnedMesh(const aiScene &ascene, int mesh_id) : m_data(ascene, mesh_id) {}

void SkinnedMesh::draw(Renderer &out, int anim_id, float anim_pos, const Material &material,
					   const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	vector<Matrix4> joints(m_data.skeleton().size());
	vector<Matrix4> inv_binds = m_data.skeleton().invBinds();

	m_data.animate(joints.data(), anim_id, anim_pos);
	m_data.drawSkeleton(out, anim_id, anim_pos, material.color());

	vector<float3> positions = m_data.positions();
	vector<float2> tex_coords = m_data.texCoords();

	m_data.animateVertices(joints.data(), positions.data(), nullptr);

	/*	m_bind_scale = float3(1, 1, 1);
		for(int n = 0; n < (int)joint_indices.size(); n++) {
			Matrix4 &inv_bind_matrix = m_inv_bind[joint_indices[n]];
			inv_bind_matrix = skin.m_inv_bind_poses->toMatrix(n) * skin.m_bind_shape_matrix;
			root.fixUpAxis(inv_bind_matrix);
			m_bind_scale = min(m_bind_scale, float3(length(inv_bind_matrix[0].xyz()),
													length(inv_bind_matrix[1].xyz()),
													length(inv_bind_matrix[2].xyz())));
		}
		m_bind_scale = inv(m_bind_scale);*/

	SimpleMeshData new_mesh(positions, tex_coords, m_data.indices());
	SimpleMesh(new_mesh).draw(out, material, Matrix4::identity());

	out.popViewMatrix();
}
}
