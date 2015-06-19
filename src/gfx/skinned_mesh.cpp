/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <map>
#include <algorithm>
#include <functional>

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

Skeleton::Skeleton(const Mesh &mesh) {
	for(const auto &node : mesh.nodes())
		m_joints.emplace_back(Joint{node.trans, node.name, node.parent_id});
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
SkeletalAnim::Channel::Channel(const XMLNode &node) {
	joint_name = node.attrib("joint_name");
	joint_id = node.attrib<int>("joint_id");

	XMLNode pos_trans_node = node.child("pos_trans");
	XMLNode scale_trans_node = node.child("scale_trans");
	XMLNode rot_trans_node = node.child("rot_trans");
	XMLNode times_node = node.child("times");
	ASSERT(pos_trans_node && scale_trans_node && rot_trans_node && times_node);

	using namespace xml_conversions;
	auto pos_trans = fromString<vector<float3>>(pos_trans_node.value());
	auto scale_trans = fromString<vector<float3>>(scale_trans_node.value());
	auto rot_trans = fromString<vector<Quat>>(rot_trans_node.value());
	auto times = fromString<vector<float>>(times_node.value());
	ASSERT(pos_trans.size() == scale_trans.size());
	ASSERT(rot_trans.size() == pos_trans.size());
	ASSERT(times.size() == pos_trans.size());

	keys.resize(times.size());
	for(int n = 0; n < (int)times.size(); n++)
		keys[n] = Key{Trans{scale_trans[n], pos_trans[n], rot_trans[n]}, times[n]};
}

void SkeletalAnim::Channel::saveToXML(XMLNode node) const {
	vector<float3> pos_trans;
	vector<float3> scale_trans;
	vector<Quat> rot_trans;
	vector<float> times;

	node.addAttrib("joint_name", node.own(joint_name));
	node.addAttrib("joint_id", joint_id);

	for(const auto &key : keys) {
		pos_trans.emplace_back(key.trans.pos);
		rot_trans.emplace_back(key.trans.rot);
		scale_trans.emplace_back(key.trans.scale);
		times.emplace_back(key.time);
	}

	using namespace xml_conversions;
	node.addChild("pos_trans", node.own(toString(pos_trans)));
	node.addChild("scale_trans", node.own(toString(scale_trans)));
	node.addChild("rot_trans", node.own(toString(rot_trans)));
	node.addChild("times", node.own(toString(times)));
}

SkeletalAnim::SkeletalAnim(const XMLNode &node, const Skeleton &skeleton)
	: m_name(node.attrib("name")), m_length(node.attrib<float>("length")) {
	XMLNode channel_node = node.child("channel");
	while(channel_node) {
		m_channels.emplace_back(channel_node);
		channel_node.next();
	}
}

void SkeletalAnim::saveToXML(XMLNode node) const {
	node.addAttrib("length", m_length);
	node.addAttrib("name", node.own(m_name));
	for(const auto &channel : m_channels)
		channel.saveToXML(node.addChild("channel"));
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

string SkeletalAnim::print() const {
	TextFormatter out;
	out("Anim: %s:", m_name.c_str());
	for(const auto &channel : m_channels) {
		out("  %12s: %d|", channel.joint_name.c_str(), (int)channel.keys.size());
		for(const auto &key : channel.keys)
			out("%6.3f ", key.time);
		out("\n");
	}
	return out.text();
}

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

SkinnedMesh::SkinnedMesh(const aiScene &ascene) : Mesh(ascene), m_skeleton(*this) {
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

SkinnedMesh::SkinnedMesh(const XMLNode &node) : Mesh(node), m_skeleton(*this) {
	XMLNode anim_node = node.child("anim");
	while(anim_node) {
		m_anims.emplace_back(anim_node, m_skeleton);
		anim_node.next();
	}

	XMLNode skin_node = node.child("skin");
	while(skin_node) {
		m_mesh_skins.emplace_back(skin_node);
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
		m_bind_matrices = vector<Matrix4>(m_skeleton.size(), Matrix4::identity());
		m_inv_bind_matrices = m_bind_matrices;
	}

	computeBoundingBox();
	verifyData();
}

void SkinnedMesh::verifyData() {
	ASSERT((int)m_bind_matrices.size() == m_skeleton.size());
	ASSERT(m_mesh_skins.size() == m_meshes.size());
}

void SkinnedMesh::saveToXML(XMLNode node) const {
	Mesh::saveToXML(node);
	if(m_bind_scale != float3(1, 1, 1))
		node.addAttrib("bind_scale", m_bind_scale);

	for(const auto &anim : m_anims)
		anim.saveToXML(node.addChild("anim"));
	for(const auto &mesh_skin : m_mesh_skins)
		mesh_skin.saveToXML(node.addChild("skin"));

	if(std::any_of(begin(m_bind_matrices), end(m_bind_matrices),
				   [](const auto &mat) { return mat != Matrix4::identity(); }))
		node.addChild("bind_matrices", node.own(xml_conversions::toString(m_bind_matrices)));
}

void SkinnedMesh::drawSkeleton(Renderer &out, const SkeletonPose &pose, Color color) const {
	SimpleMesh bbox_mesh(MakeBBox(), FBox(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f));
	out.pushViewMatrix();

	Matrix4 matrix = Matrix4::identity();

	vector<float3> positions(m_skeleton.size());
	for(int n = 0; n < (int)m_skeleton.size(); n++) {
		positions[n] = mulPoint(pose[n], m_bind_matrices[n][3].xyz());
		positions[n] = mulPoint(matrix, positions[n]);
	}

	for(int n = 0; n < (int)m_skeleton.size(); n++)
		if(m_inv_bind_matrices[n] != Matrix4::identity())
			bbox_mesh.draw(out, {Color::green}, translation(positions[n]));

	out.popViewMatrix();
}

FBox SkinnedMesh::boundingBox(const SkeletonPose &pose) const {
	FBox out = FBox::empty();

	DASSERT(!m_meshes.empty());
	for(int n = 0; n < (int)m_meshes.size(); n++) {
		DASSERT(m_meshes[n].vertexCount() > 0);
		PodArray<float3> positions(m_meshes[n].vertexCount());
		animateVertices(n, pose, positions.data(), nullptr);
		FBox bbox(positions);
		out = n == 0 ? bbox : sum(m_bounding_box, bbox);
	}

	return out;
}

void SkinnedMesh::computeBoundingBox() {
	// TODO: make this more accurate
	FBox bbox = boundingBox(animateSkeleton(-1, 0.0f));
	for(int a = 0; a < animCount(); a++)
		bbox = sum(bbox, boundingBox(animateSkeleton(a, 0.0f)));
	m_bounding_box = FBox(bbox.center() - bbox.size(), bbox.center() + bbox.size());
}

float SkinnedMesh::intersect(const Segment &segment, const SkeletonPose &pose) const {
	float min_isect = constant::inf;

	for(int n = 0; n < (int)m_meshes.size(); n++) {
		const auto &mesh = m_meshes[n];
		PodArray<float3> positions(mesh.vertexCount());
		animateVertices(n, pose, positions.data(), nullptr);

		if(intersection(segment, FBox(positions)) < constant::inf)
			for(const auto &tri : mesh.trisIndices()) {
				float isect = intersection(
					segment, Triangle(positions[tri[0]], positions[tri[1]], positions[tri[2]]));
				min_isect = min(min_isect, isect);
			}
	}

	return min_isect;
}

void SkinnedMesh::animateVertices(int mesh_id, const SkeletonPose &pose, float3 *out_positions,
								  float3 *out_normals) const {
	DASSERT(mesh_id >= 0 && mesh_id < (int)m_meshes.size());
	DASSERT((int)pose.size() == m_skeleton.size());
	const MeshSkin &skin = m_mesh_skins[mesh_id];
	const SimpleMesh &mesh = m_meshes[mesh_id];

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

SimpleMesh SkinnedMesh::animateMesh(int mesh_id, const SkeletonPose &pose) const {
	DASSERT((int)pose.size() == m_skeleton.size());
	vector<float3> positions = m_meshes[mesh_id].positions();
	vector<float2> tex_coords = m_meshes[mesh_id].texCoords();
	animateVertices(mesh_id, pose, positions.data(), nullptr);
	return SimpleMesh(positions, {}, tex_coords, m_meshes[mesh_id].indices());
}

SkeletonPose SkinnedMesh::animateSkeleton(int anim_id, double anim_pos) const {
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

void SkinnedMesh::draw(Renderer &out, const SkeletonPose &pose, const Material &material,
					   const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);
	for(int m = 0; m < (int)meshes().size(); m++)
		SimpleMesh(animateMesh(m, pose)).draw(out, material, Matrix4::identity());
	out.popViewMatrix();
	// m_data.drawSkeleton(out, anim_id, anim_pos, material.color());
}

Matrix4 SkinnedMesh::nodeTrans(const string &name, const SkeletonPose &pose) const {
	for(int n = 0; n < (int)m_skeleton.size(); n++)
		if(m_skeleton[n].name == name)
			return pose[n];
	return Matrix4::identity();
}

void SkinnedMesh::printHierarchy() const {
	for(int n = 0; n < m_skeleton.size(); n++)
		printf("%d: %s\n", n, m_skeleton[n].name.c_str());
}
}
