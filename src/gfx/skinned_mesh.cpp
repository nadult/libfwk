/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"

namespace fwk {

	/*

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

void Skeleton::loadFromXML(vector<string> &sids, XMLNode node, const collada::Root &root) {
	clear();
	m_joints.reserve(max_joints);
	loadNode(sids, node, root, -1);
}

void Skeleton::loadNode(vector<string> &sids, XMLNode node, const collada::Root &root,
						int parent_id) {
	// TODO: move skeleton XML parsing to gfx/collada
	Joint joint;
	joint.name = node.attrib("id");
	joint.parent_id = parent_id;

	XMLNode matrix_node = node.child("matrix");
	ASSERT(matrix_node);
	Matrix4 matrix;
	matrix_node.parseValues(&matrix[0][0], 16);
	matrix = transpose(matrix);
	root.fixUpAxis(matrix);
	joint.trans = matrix;

	int joint_id = (int)m_joints.size();
	m_joints.push_back(joint);
	sids.push_back(node.weakAttrib("sid"));

	XMLNode child_node = node.child("node");
	while(child_node) {
		loadNode(sids, child_node, root, joint_id);
		child_node = child_node.sibling("node");
	}
}

int Skeleton::findJoint(const string &name) const {
	for(int n = 0; n < size(); n++)
		if(m_joints[n].name == name)
			return n;
	return -1;
}

void Skeleton::clear() { m_joints.clear(); }

void SkeletalAnim::clear() {
	m_name.clear();
	m_transforms.clear();
	m_times.clear();
	m_joint_names.clear();
	m_frame_count = 0;
	m_joint_count = 0;
	m_length = 0.0f;
}

void SkeletalAnim::load(const collada::Root &root, int anim_id) {
	clear();

	using namespace collada;
	const Animation &anim = root.anim(anim_id);
	m_name = anim.id();

	m_frame_count = anim.m_frame_count;
	m_joint_count = (int)anim.m_channels.size();
	m_joint_names.resize(m_joint_count);
	DASSERT(m_frame_count > 0);

	m_times.resize(m_frame_count);
	m_transforms.resize(m_frame_count * m_joint_count);

	for(int n = 0; n < (int)anim.m_channels.size(); n++) {
		const Animation::Channel &channel = anim.m_channels[n];
		const Animation::Sampler &sampler = anim.m_samplers[channel.sampler_id];
		m_joint_names[n] =
			string(channel.target_name.c_str(), channel.target_name.find("/matrix").c_str());

		if(n == 0)
			for(int f = 0; f < m_frame_count; f++) {
				m_times[f] = sampler.input->toFloat(f);
				if(f > 0)
					ASSERT(m_times[f] > m_times[f - 1]);
			}

		for(int f = 0; f < m_frame_count; f++) {
			ASSERT(m_times[f] == sampler.input->toFloat(f));
			Matrix4 mat = sampler.output->toMatrix(f);
			root.fixUpAxis(mat);
			m_transforms[f * m_joint_count + n] = Trans(mat);
		}
	}

	for(int f = m_frame_count - 1; f >= 0; f--)
		m_times[f] -= m_times[0];

	m_length = m_times.back();
}

void SkeletalAnim::animate(Matrix4 *out, const Skeleton &skeleton, double anim_time) const {
	if(anim_time >= m_length)
		anim_time -= double(int(anim_time / m_length)) * m_length;

	int frame0 = 0, frame1 = 0;

	// TODO: binary search
	while(frame1 < m_frame_count && anim_time > m_times[frame1]) {
		frame0 = frame1;
		frame1++;
	}

	float diff = m_times[frame1] - m_times[frame0];
	float blend_factor = diff < constant::epsilon ? 0.0f : (anim_time - m_times[frame0]) / diff;

	int joint_mapping[Skeleton::max_joints];
	for(int n = 0; n < m_joint_count; n++) {
		int joint_id = -1;
		for(int i = 0; i < skeleton.size(); i++)
			if(skeleton[i].name == m_joint_names[n]) {
				joint_id = i;
				break;
			}
		ASSERT(joint_id != -1);
		joint_mapping[n] = joint_id;
	}

	for(int n = 0; n < skeleton.size(); n++)
		out[n] = (Matrix4)skeleton[n].trans;

	for(int n = 0; n < m_joint_count; n++) {
		const Trans &trans0 = m_transforms[frame0 * m_joint_count + n];
		const Trans &trans1 = m_transforms[frame1 * m_joint_count + n];
		out[joint_mapping[n]] = lerp(trans0, trans1, blend_factor);
	}

	for(int n = 0; n < skeleton.size(); n++)
		if(skeleton[n].parent_id != -1)
			out[n] = out[skeleton[n].parent_id] * out[n];
}

SkinnedMeshData::SkinnedMeshData() : m_bind_scale(1, 1, 1) {}

void SkinnedMeshData::clear() {
	m_weights.clear();
	m_ids.clear();
	m_skeleton.clear();
	m_anims.clear();
	m_inv_bind.clear();
	m_bind_scale = float3(1, 1, 1);
}

void SkinnedMeshData::load(const collada::Root &root, int mesh_id) {
	clear();

	Mesh::load(root, mesh_id);

	using namespace collada;

	ASSERT(root.sceneNodeCount() > 0);
	ASSERT(root.skinCount() > 0);
	const Skin &skin = root.skin(0);
	const Triangles &tris = root.mesh(0).triangles();

	vector<WeakString> joint_sids;
	vector<int> joint_indices(skin.m_joints->size());
	joint_sids.reserve(Skeleton::max_joints);

	for(int n = 0; n < root.sceneNodeCount(); n++) {
		XMLNode node = root.sceneNode(n).node();
		if(node.weakAttrib("type") == "JOINT") {
			m_skeleton.loadFromXML(joint_sids, node, root);
			break;
		}
	}
	ASSERT(m_skeleton.size() != 0);

	for(int n = 0; n < (int)joint_indices.size(); n++) {
		WeakString joint_name = skin.m_joints->toString(n);
		int joint_id = -1;
		for(int i = 0; i < (int)joint_sids.size(); i++)
			if(joint_name == joint_sids[i]) {
				joint_id = i;
				break;
			}
		ASSERT(joint_id != -1);
		joint_indices[n] = joint_id;
	}

	m_inv_bind.resize(m_skeleton.size());
	for(int n = 0; n < m_inv_bind.size(); n++)
		m_inv_bind[n] = identity();

	m_bind_scale = float3(1, 1, 1);
	for(int n = 0; n < (int)joint_indices.size(); n++) {
		Matrix4 &inv_bind_matrix = m_inv_bind[joint_indices[n]];
		inv_bind_matrix = skin.m_inv_bind_poses->toMatrix(n) * skin.m_bind_shape_matrix;
		root.fixUpAxis(inv_bind_matrix);
		m_bind_scale = min(m_bind_scale, float3(length(inv_bind_matrix[0].xyz()),
												length(inv_bind_matrix[1].xyz()),
												length(inv_bind_matrix[2].xyz())));
	}
	m_bind_scale = inv(m_bind_scale);

	PodArray<JointWeight> tmp_weights(skin.m_counts.size());
	PodArray<JointId> tmp_ids(skin.m_counts.size());
	bool weights_warning = false;

	int voffset = 0;
	for(int n = 0; n < (int)tmp_weights.size(); n++) {
		JointWeight weight;
		JointId id;

		memset(&weight, 0, sizeof(weight));
		memset(&id, 0, sizeof(id));
		float weight_sum = 0.0;

		for(int i = 0; i < skin.m_counts[n]; i++) {
			if(i >= max_weights)
				THROW("Too many bones assigned to vertex (limit is: %d)\n", max_weights);

			id.v[i] = joint_indices[skin.m_indices[voffset + 0]];
			weight.v[i] = skin.m_weights->toFloat(skin.m_indices[voffset + 1]);
			weight_sum += weight.v[i];
			voffset += 2;
		}
		for(int i = 0; i < max_weights; i++)
			weight.v[i] /= weight_sum;

		tmp_weights[n] = weight;
		tmp_ids[n] = id;
	}

	m_weights.resize(m_positions.size());
	m_ids.resize(m_positions.size());

	for(int v = 0; v < tris.vertexCount(); v++) {
		int vindex = tris.attribIndex(Semantic::vertex, v);
		m_weights[v] = tmp_weights[vindex];
		m_ids[v] = tmp_ids[vindex];
	}

	m_anims.resize(root.animCount());
	for(int n = 0; n < root.animCount(); n++)
		m_anims[n].load(root, n);

	m_bind.resize(m_inv_bind.size());
	for(int n = 0; n < (int)m_bind.size(); n++)
		m_bind[n] = inverse(m_inv_bind[n]);
	computeBBox();
}

const float3 SkinnedMeshData::transformPoint(int id, const Matrix4 *joints) const {
	float3 out(0, 0, 0);
	for(int n = 0; n < max_weights; n++)
		if(m_weights[id].v[n] != 0.0f)
			out += mulPoint(joints[m_ids[id].v[n]], m_positions[id]) * m_weights[id].v[n];
	return float3(out.x, out.z, out.y);
}

const float3 SkinnedMeshData::rootPos(int anim_id, double anim_time) const {
	// TODO: speed up
	Matrix4 joints[Skeleton::max_joints];
	animate(joints, anim_id, anim_time);
	return mulPoint(joints[0], m_bind[0][3].xyz());
}

void SkinnedMeshData::renderSkeleton(Renderer &out, const Matrix4 *joints, const Matrix4 &trans,
									 Color color) const {
	float3 positions[Skeleton::max_joints];
	computeJointPositions(joints, trans, positions);

	for(int n = 0; n < m_skeleton.size(); n++) {
		out.addBox(FBox(-1, -1, -1, 1, 1, 1) * 0.1f, translation(positions[n]), true, color);
		if(m_skeleton[n].parent_id != -1)
			out.addLine(positions[n], positions[m_skeleton[n].parent_id], color);
	}
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

	for(int n = 0; n < anim.frameCount(); n++) {
		Matrix4 anim_pose[Skeleton::max_joints];
		animate(anim_pose, anim_id, anim.frameTime(n));
		float error;
		float3 offset;

		alignPoses(pose, anim_pose, pose_trans, identity(), error, offset);
		//	printf("Frame %d: err:%f offset: ", n, error);
		//	print(offset);
		//	printf("\n");

		if(error < min_error) {
			min_error = error;
			min_offset = offset;
			min_pos = anim.frameTime(n);
		}
	}

	out_pos = min_pos;
	out_offset = min_offset;
	//	printf("Best: pos:%f off:(%.2f %.2f %.2f) err:%f\n",
	//			out_pos, out_offset.x, out_offset.y, out_offset.z, min_error);
}

void SkinnedMeshData::computeBBox() {
	PodArray<float3> temp(vertexCount());

	for(int a = 0; a < (int)m_anims.size(); a++) {
		const SkeletalAnim &anim = m_anims[a];
		int frame_count = anim.frameCount();
		for(int f = 0; f < frame_count; f++) {
			Matrix4 joints[Skeleton::max_joints];

			anim.animate(joints, m_skeleton, double(f) * anim.length() / double(frame_count - 1));
			for(int j = 0; j < m_skeleton.size(); j++)
				joints[j] = scaling(m_bind_scale) * joints[j] * m_inv_bind[j];

			animateVertices(joints, temp.data());
			FBox bbox(temp.data(), temp.size());

			if(a == 0 && f == 0)
				m_bbox = bbox;
			else
				m_bbox = FBox(min(m_bbox.min, bbox.min), max(m_bbox.max, bbox.max));
		}
	}
}

void SkinnedMeshData::animateVertices(const Matrix4 *joints, float3 *out_positions,
									  float3 *out_normals) const {
	Matrix4 temp_joints[Skeleton::max_joints];
	if(!joints) {
		animate(temp_joints, 0, 0.0);
		joints = temp_joints;
	}

	for(int n = 0; n < m_positions.size(); n++) {
		const float3 &pos = m_positions[n];
		const float3 &nrm = m_normals[n];
		const JointWeight &weight = m_weights[n];
		const JointId &id = m_ids[n];

		if(out_positions) {
			float3 out(0, 0, 0);
			for(int i = 0; i < max_weights; i++)
				if(weight.v[i] > 0.0f)
					out += mulPointAffine(joints[id.v[i]], pos) * weight.v[i];
			out_positions[n] = out;
		}

		if(out_normals) {
			float3 out(0, 0, 0);
			for(int i = 0; i < max_weights; i++)
				if(weight.v[i] > 0.0f)
					out += mulNormalAffine(joints[id.v[i]], nrm) * weight.v[i];
			out_normals[n] = out;
		}
	}
}

void SkinnedMeshData::computeJointPositions(const Matrix4 *joints, const Matrix4 &trans,
											float3 *out) const {
	for(int n = 0; n < m_skeleton.size(); n++) {
		out[n] = mulPoint(joints[n], m_bind[n][3].xyz());
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
		trans[n] = scaling(m_bind_scale) * trans[n] * m_inv_bind[n];

	if(m_anims[anim_id].name() == "anim_standing_rifle_aim" ||
	   m_anims[anim_id].name() == "anim_standing_rifle_fire")
		for(int n = 0; n < m_skeleton.size(); n++)
			trans[n] = rotation(float3(0, 1, 0), constant::pi) * trans[n];
}

*/
}
