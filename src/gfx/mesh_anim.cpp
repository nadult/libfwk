/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_profile.h"
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

void MeshAnim::transToXML(const AffineTrans &trans, XMLNode node) {
	if(!areSimilar(trans.translation, float3()))
		node.addAttrib("pos", trans.translation);
	if(!areSimilar(trans.scale, float3(1, 1, 1)))
		node.addAttrib("scale", trans.scale);
	if(!areSimilar((float4)trans.rotation, (float4)Quat()))
		node.addAttrib("rot", trans.rotation);
}

AffineTrans MeshAnim::transFromXML(XMLNode node) {
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

MeshPose::MeshPose(vector<AffineTrans> transforms)
	: m_transforms(std::move(transforms)), m_is_dirty(true), m_is_skinned_dirty(true) {}

void MeshPose::transform(const AffineTrans &pre_trans) {
	for(auto &trans : m_transforms)
		trans = pre_trans * trans;
	m_is_dirty = m_is_skinned_dirty = true;
}

MeshAnim::Channel::Channel(const XMLNode &node)
	: joint_name(node.attrib("joint_name")), joint_id(node.attrib<int>("joint_id")) {
	using namespace xml_conversions;

	default_trans = transFromXML(node);
	if(XMLNode pos_node = node.child("pos"))
		translation_track = fromString<vector<float3>>(pos_node.value());
	if(XMLNode scale_node = node.child("scale"))
		scaling_track = fromString<vector<float3>>(scale_node.value());
	if(XMLNode rot_node = node.child("rot"))
		rotation_track = fromString<vector<Quat>>(rot_node.value());
	if(XMLNode times_node = node.child("time"))
		time_track = fromString<vector<float>>(times_node.value());
}

void MeshAnim::Channel::saveToXML(XMLNode node) const {
	node.addAttrib("joint_name", node.own(joint_name));
	node.addAttrib("joint_id", joint_id);

	using namespace xml_conversions;

	transToXML(default_trans, node);
	if(!translation_track.empty())
		node.addChild("pos", node.own(toString(translation_track)));
	if(!scaling_track.empty())
		node.addChild("scale", node.own(toString(scaling_track)));
	if(!rotation_track.empty())
		node.addChild("rot", node.own(toString(rotation_track)));
	if(!time_track.empty())
		node.addChild("time", node.own(toString(time_track)));
}

aiNodeAnim *MeshAnim::Channel::toAINodeAnim(const vector<float> &shared_time_track) const {
	auto *out = new aiNodeAnim;

	out->mNumPositionKeys = translation_track.size();
	out->mNumRotationKeys = rotation_track.size();
	out->mNumScalingKeys = scaling_track.size();

	out->mPositionKeys =
		translation_track.empty() ? nullptr : new aiVectorKey[translation_track.size()];
	out->mRotationKeys = rotation_track.empty() ? nullptr : new aiQuatKey[rotation_track.size()];
	out->mScalingKeys = scaling_track.empty() ? nullptr : new aiVectorKey[scaling_track.size()];

	const auto &time_track = this->time_track.empty() ? shared_time_track : this->time_track;

	for(int n = 0; n < (int)translation_track.size(); n++) {
		float3 pos = translation_track[n];
		out->mPositionKeys[n].mTime = time_track[n];
		out->mPositionKeys[n].mValue = aiVector3D(pos.x, pos.y, pos.z);
	}

	for(int n = 0; n < (int)scaling_track.size(); n++) {
		float3 scale = scaling_track[n];
		out->mScalingKeys[n].mTime = time_track[n];
		out->mScalingKeys[n].mValue = aiVector3D(scale.x, scale.y, scale.z);
	}

	for(int n = 0; n < (int)rotation_track.size(); n++) {
		auto rot = rotation_track[n];
		out->mRotationKeys[n].mTime = time_track[n];
		out->mRotationKeys[n].mValue = aiQuaternion(rot.w, rot.x, rot.y, rot.z);
	}

	out->mPreState = out->mPostState = aiAnimBehaviour::aiAnimBehaviour_CONSTANT;
	out->mNodeName = joint_name.c_str();

	return out;
}

MeshAnim::Channel::Channel(const aiNodeAnim &achannel, int joint_id,
						   vector<float> &shared_time_track)
	: joint_name(achannel.mNodeName.C_Str()), joint_id(joint_id) {
	ASSERT(achannel.mNumRotationKeys == achannel.mNumPositionKeys);
	ASSERT(achannel.mNumScalingKeys == achannel.mNumPositionKeys);
	translation_track.resize(achannel.mNumPositionKeys);
	scaling_track.resize(achannel.mNumPositionKeys);
	rotation_track.resize(achannel.mNumPositionKeys);
	time_track.resize(achannel.mNumPositionKeys);

	for(uint k = 0; k < achannel.mNumPositionKeys; k++) {
		ASSERT(achannel.mPositionKeys[k].mTime == achannel.mRotationKeys[k].mTime);
		ASSERT(achannel.mScalingKeys[k].mTime == achannel.mRotationKeys[k].mTime);

		translation_track[k] = convert(achannel.mPositionKeys[k].mValue);
		scaling_track[k] = convert(achannel.mScalingKeys[k].mValue);
		rotation_track[k] = convert(achannel.mRotationKeys[k].mValue);
		time_track[k] = achannel.mPositionKeys[k].mTime;
	}

	if(allOf(translation_track, [](auto v) { return areSimilar(v, float3()); }))
		translation_track.clear();
	if(allOf(scaling_track, [](auto v) { return areSimilar(v, float3(1, 1, 1)); }))
		scaling_track.clear();
	if(allOf(rotation_track, [](auto q) { return areSimilar((float4)q, (float4)Quat()); }))
		rotation_track.clear();

	if(shared_time_track.empty())
		shared_time_track = time_track;
	if(time_track == shared_time_track)
		time_track.clear();
}

AffineTrans MeshAnim::Channel::blend(int frame0, int frame1, float t) const {
	AffineTrans out = default_trans;

	if(!translation_track.empty())
		out.translation = lerp(translation_track[frame0], translation_track[frame1], t);
	if(!scaling_track.empty())
		out.scale = lerp(scaling_track[frame0], scaling_track[frame1], t);
	if(!rotation_track.empty())
		out.rotation = slerp(rotation_track[frame0], rotation_track[frame1], t);

	return out;
}

MeshAnim::MeshAnim(const aiScene &ascene, int anim_id, const Mesh &mesh) : m_max_joint_id(0) {
	DASSERT(anim_id >= 0 && anim_id < (int)ascene.mNumAnimations);

	const auto &aanim = *ascene.mAnimations[anim_id];

	for(uint c = 0; c < aanim.mNumChannels; c++) {
		const auto &achannel = *aanim.mChannels[c];

		int joint_id = mesh.findNode(achannel.mNodeName.C_Str());
		if(joint_id == -1)
			continue;
		m_max_joint_id = max(m_max_joint_id, joint_id);
		m_channels.emplace_back(achannel, joint_id, m_shared_time_track);
	}

	m_name = aanim.mName.C_Str();
	m_length = aanim.mDuration;
	verifyData();
}

MeshAnim::MeshAnim(const XMLNode &node)
	: m_name(node.attrib("name")), m_max_joint_id(0), m_length(node.attrib<float>("length")) {
	XMLNode channel_node = node.child("channel");
	while(channel_node) {
		m_channels.emplace_back(channel_node);
		m_max_joint_id = max(m_max_joint_id, m_channels.back().joint_id);
		channel_node.next();
	}

	XMLNode shared_track = node.child("shared_time_track");
	ASSERT(shared_track);
	m_shared_time_track = xml_conversions::fromString<vector<float>>(shared_track.value());
	verifyData();
}

void MeshAnim::saveToXML(XMLNode node) const {
	node.addAttrib("length", m_length);
	node.addAttrib("name", node.own(m_name));
	for(const auto &channel : m_channels)
		channel.saveToXML(node.addChild("channel"));
	node.addChild("shared_time_track", node.own(xml_conversions::toString(m_shared_time_track)));
}

aiAnimation *MeshAnim::toAIAnimation() const {
	auto *out = new aiAnimation;

	out->mNumChannels = m_channels.size();
	out->mChannels = m_channels.empty() ? nullptr : new aiNodeAnim *[out->mNumChannels];
	for(int n = 0; n < (int)m_channels.size(); n++)
		out->mChannels[n] = m_channels[n].toAINodeAnim(m_shared_time_track);
	out->mName = m_name.c_str();
	out->mDuration = m_length;

	return out;
}

AffineTrans MeshAnim::animateChannel(int channel_id, double anim_pos) const {
	DASSERT(channel_id >= 0 && channel_id < (int)m_channels.size());
	const auto &channel = m_channels[channel_id];

	const auto &times = channel.time_track.empty() ? m_shared_time_track : channel.time_track;

	int frame0 = 0, frame1 = 0, frame_count = (int)times.size();
	// TODO: binary search
	while(frame1 < frame_count && anim_pos > times[frame1]) {
		frame0 = frame1;
		frame1++;
	}

	float diff = times[frame1] - times[frame0];
	float blend_factor = diff < constant::epsilon ? 0.0f : (anim_pos - times[frame0]) / diff;

	return channel.blend(frame0, frame1, blend_factor);
}

MeshPose MeshAnim::animatePose(MeshPose pose, double anim_pos) const {
	DASSERT(m_max_joint_id < (int)pose.size());

	if(anim_pos >= m_length)
		anim_pos -= double(int(anim_pos / m_length)) * m_length;

	for(int n = 0; n < (int)m_channels.size(); n++)
		pose.m_transforms[m_channels[n].joint_id] = animateChannel(n, anim_pos);

	return pose;
}

string MeshAnim::print() const {
	TextFormatter out;
	out("Anim: %s:", m_name.c_str());
	for(const auto &channel : m_channels) {
		const auto &time_track =
			channel.time_track.empty() ? m_shared_time_track : channel.time_track;
		out("  %12s: %d|", channel.joint_name.c_str(), (int)time_track.size());
		for(float time : time_track)
			out("%6.3f ", time);
		out("\n");
	}
	return out.text();
}

void MeshAnim::verifyData() const {
	for(int n = 0; n < (int)m_channels.size(); n++) {
		const auto &channel = m_channels[n];
		auto num_keys =
			channel.time_track.empty() ? m_shared_time_track.size() : channel.time_track.size();
		ASSERT(channel.translation_track.size() == num_keys || channel.translation_track.empty());
		ASSERT(channel.rotation_track.size() == num_keys || channel.rotation_track.empty());
		ASSERT(channel.scaling_track.size() == num_keys || channel.scaling_track.empty());
		ASSERT(channel.joint_id >= 0);
	}
}
}
