// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/model_anim.h"

#include "fwk/gfx/pose.h"
#include "fwk/sys/xml.h"
#include "fwk_profile.h"

namespace fwk {

template <class T> bool closeEnough(const T &a, const T &b) {
	auto dist = distanceSq(a, b);
	return dist < epsilon<decltype(dist)>;
}

ModelAnim::ModelAnim() = default;
FWK_COPYABLE_CLASS_IMPL(ModelAnim);

void ModelAnim::transToXML(const AffineTrans &trans, const AffineTrans &default_trans,
						   XmlNode node) {
	if(!closeEnough(trans.translation, default_trans.translation))
		node.addAttrib("pos", trans.translation);
	if(!closeEnough(trans.scale, default_trans.scale))
		node.addAttrib("scale", trans.scale);
	if(!closeEnough((float4)trans.rotation, (float4)default_trans.rotation))
		node.addAttrib("rot", trans.rotation);
}

AffineTrans ModelAnim::transFromXML(CXmlNode node, const AffineTrans &default_trans) {
	float3 pos = default_trans.translation, scale = default_trans.scale;
	Quat rot = default_trans.rotation;

	if(auto pos_string = node.hasAttrib("pos"))
		pos = fromString<float3>(pos_string);
	if(auto scale_string = node.hasAttrib("scale"))
		scale = fromString<float3>(scale_string);
	if(auto rot_string = node.hasAttrib("rot"))
		rot = fromString<Quat>(rot_string);
	return AffineTrans(pos, rot, scale);
}

Ex<void> ModelAnim::Channel::load(CXmlNode node, const AffineTrans &default_trans) {
	this->default_trans = default_trans;
	node_name = node.attrib("name");
	trans = transFromXML(node, default_trans);

	if(auto pos_node = node.child("pos"))
		translation_track = fromString<vector<float3>>(pos_node.value());
	if(auto scale_node = node.child("scale"))
		scaling_track = fromString<vector<float3>>(scale_node.value());
	if(auto rot_node = node.child("rot"))
		rotation_track = fromString<vector<Quat>>(rot_node.value());
	if(auto times_node = node.child("time"))
		time_track = fromString<vector<float>>(times_node.value());

	EXPECT_NO_ERRORS();
	return {};
}

void ModelAnim::Channel::save(XmlNode node) const {
	node.addAttrib("name", node.own(node_name));

	transToXML(trans, default_trans, node);
	if(translation_track)
		node.addChild("pos", translation_track);
	if(scaling_track)
		node.addChild("scale", scaling_track);
	if(rotation_track)
		node.addChild("rot", rotation_track);
	if(time_track)
		node.addChild("time", time_track);
}

AffineTrans ModelAnim::Channel::blend(int frame0, int frame1, float t) const {
	AffineTrans out = trans;

	if(translation_track)
		out.translation = lerp(translation_track[frame0], translation_track[frame1], t);
	if(scaling_track)
		out.scale = lerp(scaling_track[frame0], scaling_track[frame1], t);
	if(rotation_track)
		out.rotation = slerp(rotation_track[frame0], rotation_track[frame1], t);

	return out;
}

Ex<ModelAnim> ModelAnim::load(CXmlNode node, PPose default_pose) {
	ModelAnim out;

	out.m_name = node.attrib("name");
	out.m_length = node.attrib<float>("length");

	auto channel_node = node.child("channel");
	while(channel_node) {
		out.m_node_names.emplace_back(channel_node.attrib("name"));
		channel_node.next();
	}

	auto transforms = default_pose->mapTransforms(default_pose->mapNames(out.m_node_names));
	auto trans_it = transforms.begin();

	channel_node = node.child("channel");
	while(channel_node) {
		Channel channel;
		EXPECT(channel.load(channel_node, AffineTrans(*trans_it++)));
		out.m_channels.emplace_back(move(channel));
		channel_node.next();
	}

	auto shared_track = node.child("shared_time_track");
	ASSERT(shared_track);
	out.m_shared_time_track = fromString<vector<float>>(shared_track.value());

	out.verifyData();
	EXPECT_NO_ERRORS();
	return move(out);
}

void ModelAnim::save(XmlNode node) const {
	node.addAttrib("length", m_length);
	node.addAttrib("name", node.own(m_name));
	for(const auto &channel : m_channels)
		channel.save(node.addChild("channel"));
	node.addChild("shared_time_track", m_shared_time_track);
}

AffineTrans ModelAnim::animateChannel(int channel_id, double anim_pos) const {
	DASSERT(channel_id >= 0 && channel_id < m_channels.size());
	const auto &channel = m_channels[channel_id];

	const auto &times = channel.time_track ? channel.time_track : m_shared_time_track;

	int frame0 = 0, frame1 = 0, frame_count = times.size();
	// TODO: binary search
	while(frame1 < frame_count && anim_pos > times[frame1]) {
		frame0 = frame1;
		frame1++;
	}
	if(frame1 == frame_count)
		frame1--;
	// TODO: fix timing

	float diff = times[frame1] - times[frame0];
	float blend_factor = diff < epsilon<float> ? 0.0f : (anim_pos - times[frame0]) / diff;

	return channel.blend(frame0, frame1, blend_factor);
}

void ModelAnim::setDefaultPose(PPose pose) {
	m_default_matrices = pose->transforms();
	m_default_mapping = pose->mapNames(m_node_names);
}

vector<Matrix4> ModelAnim::animateDefaultPose(double anim_pos) const {
	if(anim_pos >= m_length)
		anim_pos -= double(int(anim_pos / m_length)) * m_length;
	auto matrices = m_default_matrices;
	for(int n = 0; n < m_channels.size(); n++) {
		const auto &channel = m_channels[n];
		matrices[m_default_mapping[n]] = animateChannel(n, anim_pos);
	}
	return matrices;
}

PPose ModelAnim::animatePose(PPose initial_pose, double anim_pos) const {
	DASSERT(initial_pose);
	if(anim_pos >= m_length)
		anim_pos -= double(int(anim_pos / m_length)) * m_length;

	auto mapping = initial_pose->mapNames(m_node_names);
	auto matrices = initial_pose->transforms();
	for(int n = 0; n < m_channels.size(); n++) {
		const auto &channel = m_channels[n];
		matrices[mapping[n]] = animateChannel(n, anim_pos);
	}

	return make_immutable<Pose>(move(matrices), initial_pose->nameMap());
}

string ModelAnim::print() const {
	TextFormatter out;
	out("Anim: %:", m_name);
	for(const auto &channel : m_channels) {
		const auto &time_track = channel.time_track ? channel.time_track : m_shared_time_track;
		out.stdFormat("  %12s: %d|", channel.node_name.c_str(), time_track.size());
		for(float time : time_track)
			out.stdFormat("%6.3f ", time);
		out("\n");
	}
	return out.text();
}

void ModelAnim::verifyData() const {
	for(int n = 0; n < m_channels.size(); n++) {
		const auto &channel = m_channels[n];
		auto num_keys = channel.time_track ? channel.time_track.size() : m_shared_time_track.size();
		ASSERT(channel.translation_track.size() == num_keys || !channel.translation_track);
		ASSERT(channel.rotation_track.size() == num_keys || !channel.rotation_track);
		ASSERT(channel.scaling_track.size() == num_keys || !channel.scaling_track);
	}
}
}
