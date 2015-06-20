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

MeshPose::MeshPose(vector<AffineTrans> transforms)
	: m_transforms(std::move(transforms)), m_is_dirty(true), m_is_skinned_dirty(true) {}

MeshAnim::Channel::Channel(const XMLNode &node)
	: joint_name(node.attrib("joint_name")), joint_id(node.attrib<int>("joint_id")) {
	using namespace xml_conversions;
	if(XMLNode pos_node = node.child("translation"))
		translation_track = fromString<vector<float3>>(pos_node.value());
	if(XMLNode scale_node = node.child("scale"))
		scale_track = fromString<vector<float3>>(scale_node.value());
	if(XMLNode rot_node = node.child("rotation"))
		rotation_track = fromString<vector<Quat>>(rot_node.value());
	if(XMLNode times_node = node.child("time"))
		time_track = fromString<vector<float>>(times_node.value());
}

void MeshAnim::Channel::saveToXML(XMLNode node) const {
	node.addAttrib("joint_name", node.own(joint_name));
	node.addAttrib("joint_id", joint_id);

	using namespace xml_conversions;
	if(!translation_track.empty())
		node.addChild("translation", node.own(toString(translation_track)));
	if(!scale_track.empty())
		node.addChild("scale", node.own(toString(scale_track)));
	if(!rotation_track.empty())
		node.addChild("rotation", node.own(toString(rotation_track)));
	if(!time_track.empty())
		node.addChild("time", node.own(toString(time_track)));
}

MeshAnim::Channel::Channel(const aiNodeAnim &achannel, int joint_id,
						   vector<float> &shared_time_track)
	: joint_name(achannel.mNodeName.C_Str()), joint_id(joint_id) {
	ASSERT(achannel.mNumRotationKeys == achannel.mNumPositionKeys);
	ASSERT(achannel.mNumScalingKeys == achannel.mNumPositionKeys);
	translation_track.resize(achannel.mNumPositionKeys);
	scale_track.resize(achannel.mNumPositionKeys);
	rotation_track.resize(achannel.mNumPositionKeys);
	time_track.resize(achannel.mNumPositionKeys);

	for(uint k = 0; k < achannel.mNumPositionKeys; k++) {
		ASSERT(achannel.mPositionKeys[k].mTime == achannel.mRotationKeys[k].mTime);
		ASSERT(achannel.mScalingKeys[k].mTime == achannel.mRotationKeys[k].mTime);

		translation_track[k] = convert(achannel.mPositionKeys[k].mValue);
		scale_track[k] = convert(achannel.mScalingKeys[k].mValue);
		rotation_track[k] = convert(achannel.mRotationKeys[k].mValue);
		time_track[k] = achannel.mPositionKeys[k].mTime;
	}

	if(allOf(translation_track, [](auto v) { return areSimilar(v, float3()); }))
		translation_track.clear();
	if(allOf(scale_track, [](auto v) { return areSimilar(v, float3(1, 1, 1)); }))
		scale_track.clear();
	if(allOf(rotation_track, [](auto q) { return areSimilar((float4)q, (float4)Quat()); }))
		rotation_track.clear();

	if(shared_time_track.empty())
		shared_time_track = time_track;
	if(time_track == shared_time_track)
		time_track.clear();
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

AffineTrans MeshAnim::animateChannel(int channel_id, double anim_pos) const {
	DASSERT(channel_id >= 0 && channel_id < m_channels.size());
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

	float3 pos, scale(1, 1, 1);
	Quat rot;

	if(!channel.translation_track.empty())
		pos = lerp(channel.translation_track[frame0], channel.translation_track[frame1],
				   blend_factor);
	if(!channel.scale_track.empty())
		scale = lerp(channel.scale_track[frame0], channel.scale_track[frame1], blend_factor);
	if(!channel.rotation_track.empty())
		rot = slerp(channel.rotation_track[frame0], channel.rotation_track[frame1], blend_factor);

	return AffineTrans(pos, scale, rot);
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
		ASSERT(channel.scale_track.size() == num_keys || channel.scale_track.empty());
		ASSERT(channel.joint_id >= 0);
	}
}
}
