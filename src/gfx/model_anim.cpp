/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_profile.h"
#include <map>
#include <algorithm>
#include <functional>

namespace fwk {

void ModelAnim::transToXML(const AffineTrans &trans, XMLNode node) {
	if(!areSimilar(trans.translation, float3()))
		node.addAttrib("pos", trans.translation);
	if(!areSimilar(trans.scale, float3(1, 1, 1)))
		node.addAttrib("scale", trans.scale);
	if(!areSimilar((float4)trans.rotation, (float4)Quat()))
		node.addAttrib("rot", trans.rotation);
}

AffineTrans ModelAnim::transFromXML(XMLNode node) {
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

ModelPose::ModelPose(vector<AffineTrans> transforms)
	: m_transforms(std::move(transforms)), m_is_dirty(true), m_is_skinned_dirty(true) {}

void ModelPose::transform(const AffineTrans &pre_trans) {
	for(auto &trans : m_transforms)
		trans = pre_trans * trans;
	m_is_dirty = m_is_skinned_dirty = true;
}

ModelAnim::Channel::Channel(const XMLNode &node) : node_name(node.attrib("name")), node_id(-1) {
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

void ModelAnim::Channel::saveToXML(XMLNode node) const {
	node.addAttrib("name", node.own(node_name));

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

AffineTrans ModelAnim::Channel::blend(int frame0, int frame1, float t) const {
	AffineTrans out = default_trans;

	if(!translation_track.empty())
		out.translation = lerp(translation_track[frame0], translation_track[frame1], t);
	if(!scaling_track.empty())
		out.scale = lerp(scaling_track[frame0], scaling_track[frame1], t);
	if(!rotation_track.empty())
		out.rotation = slerp(rotation_track[frame0], rotation_track[frame1], t);

	return out;
}

ModelAnim::ModelAnim(const XMLNode &node, const Model &model)
	: m_name(node.attrib("name")), m_max_node_id(0), m_length(node.attrib<float>("length")) {
	XMLNode channel_node = node.child("channel");
	while(channel_node) {
		m_channels.emplace_back(channel_node);
		m_max_node_id = max(m_max_node_id, m_channels.back().node_id);
		channel_node.next();
	}

	XMLNode shared_track = node.child("shared_time_track");
	ASSERT(shared_track);
	m_shared_time_track = xml_conversions::fromString<vector<float>>(shared_track.value());

	updateNodeIndices(model);
	verifyData();
}

void ModelAnim::updateNodeIndices(const Model &model) {
	m_max_node_id = 0;
	for(auto &channel : m_channels) {
		channel.node_id = model.findNodeId(channel.node_name);
		m_max_node_id = max(m_max_node_id, channel.node_id);
	}
}

void ModelAnim::saveToXML(XMLNode node) const {
	node.addAttrib("length", m_length);
	node.addAttrib("name", node.own(m_name));
	for(const auto &channel : m_channels)
		channel.saveToXML(node.addChild("channel"));
	node.addChild("shared_time_track", node.own(xml_conversions::toString(m_shared_time_track)));
}

AffineTrans ModelAnim::animateChannel(int channel_id, double anim_pos) const {
	DASSERT(channel_id >= 0 && channel_id < (int)m_channels.size());
	const auto &channel = m_channels[channel_id];

	const auto &times = channel.time_track.empty() ? m_shared_time_track : channel.time_track;

	int frame0 = 0, frame1 = 0, frame_count = (int)times.size();
	// TODO: binary search
	while(frame1 < frame_count && anim_pos > times[frame1]) {
		frame0 = frame1;
		frame1++;
	}
	if(frame1 == frame_count)
		frame1--;
	// TODO: fix timing

	float diff = times[frame1] - times[frame0];
	float blend_factor = diff < constant::epsilon ? 0.0f : (anim_pos - times[frame0]) / diff;

	return channel.blend(frame0, frame1, blend_factor);
}

ModelPose ModelAnim::animatePose(ModelPose pose, double anim_pos) const {
	DASSERT(m_max_node_id < (int)pose.size());

	if(anim_pos >= m_length)
		anim_pos -= double(int(anim_pos / m_length)) * m_length;

	for(int n = 0; n < (int)m_channels.size(); n++)
		pose.m_transforms[m_channels[n].node_id] = animateChannel(n, anim_pos);

	return pose;
}

string ModelAnim::print() const {
	TextFormatter out;
	out("Anim: %s:", m_name.c_str());
	for(const auto &channel : m_channels) {
		const auto &time_track =
			channel.time_track.empty() ? m_shared_time_track : channel.time_track;
		out("  %12s: %d|", channel.node_name.c_str(), (int)time_track.size());
		for(float time : time_track)
			out("%6.3f ", time);
		out("\n");
	}
	return out.text();
}

void ModelAnim::verifyData() const {
	for(int n = 0; n < (int)m_channels.size(); n++) {
		const auto &channel = m_channels[n];
		auto num_keys =
			channel.time_track.empty() ? m_shared_time_track.size() : channel.time_track.size();
		ASSERT(channel.translation_track.size() == num_keys || channel.translation_track.empty());
		ASSERT(channel.rotation_track.size() == num_keys || channel.rotation_track.empty());
		ASSERT(channel.scaling_track.size() == num_keys || channel.scaling_track.empty());
		ASSERT(channel.node_id >= 0);
	}
}
}
