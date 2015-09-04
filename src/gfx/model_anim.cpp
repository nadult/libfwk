/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_profile.h"
#include <map>
#include <algorithm>
#include <functional>

namespace fwk {

void ModelAnim::transToXML(const AffineTrans &trans, const AffineTrans &default_trans,
						   XMLNode node) {
	if(!areSimilar(trans.translation, default_trans.translation))
		node.addAttrib("pos", trans.translation);
	if(!areSimilar(trans.scale, default_trans.scale))
		node.addAttrib("scale", trans.scale);
	if(!areSimilar((float4)trans.rotation, (float4)default_trans.rotation))
		node.addAttrib("rot", trans.rotation);
}

AffineTrans ModelAnim::transFromXML(XMLNode node, const AffineTrans &default_trans) {
	using namespace xml_conversions;
	float3 pos = default_trans.translation, scale = default_trans.scale;
	Quat rot = default_trans.rotation;

	if(auto *pos_string = node.hasAttrib("pos"))
		pos = fromString<float3>(pos_string);
	if(auto *scale_string = node.hasAttrib("scale"))
		scale = fromString<float3>(scale_string);
	if(auto *rot_string = node.hasAttrib("rot"))
		rot = fromString<Quat>(rot_string);
	return AffineTrans(pos, rot, scale);
}

ModelAnim::Channel::Channel(const XMLNode &node, const AffineTrans &default_trans)
	: default_trans(default_trans), node_name(node.attrib("name")) {
	using namespace xml_conversions;

	trans = transFromXML(node, default_trans);
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

	transToXML(trans, default_trans, node);
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
	AffineTrans out = trans;

	if(!translation_track.empty())
		out.translation = lerp(translation_track[frame0], translation_track[frame1], t);
	if(!scaling_track.empty())
		out.scale = lerp(scaling_track[frame0], scaling_track[frame1], t);
	if(!rotation_track.empty())
		out.rotation = slerp(rotation_track[frame0], rotation_track[frame1], t);

	return out;
}

ModelAnim::ModelAnim(const XMLNode &node, PPose default_pose)
	: m_name(node.attrib("name")), m_length(node.attrib<float>("length")) {
	XMLNode channel_node = node.child("channel");
	while(channel_node) {
		m_node_names.emplace_back(channel_node.attrib("name"));
		channel_node.next();
	}

	auto transforms = default_pose->mapTransforms(default_pose->mapNames(m_node_names));
	auto trans_it = transforms.begin();

	channel_node = node.child("channel");
	while(channel_node) {
		m_channels.emplace_back(channel_node, AffineTrans(*trans_it++));
		channel_node.next();
	}

	XMLNode shared_track = node.child("shared_time_track");
	ASSERT(shared_track);
	m_shared_time_track = xml_conversions::fromString<vector<float>>(shared_track.value());

	verifyData();
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

PPose ModelAnim::animatePose(PPose initial_pose, double anim_pos) const {
	DASSERT(initial_pose);
	if(anim_pos >= m_length)
		anim_pos -= double(int(anim_pos / m_length)) * m_length;

	auto mapping = initial_pose->mapNames(m_node_names);
	auto matrices = initial_pose->transforms();
	for(int n = 0; n < (int)m_channels.size(); n++) {
		const auto &channel = m_channels[n];
		matrices[mapping[n]] = animateChannel(n, anim_pos);
	}

	return make_immutable<Pose>(std::move(matrices), initial_pose->nameMap());
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
	}
}
}
