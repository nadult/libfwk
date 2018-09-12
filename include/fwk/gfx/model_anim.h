// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/math/affine_trans.h"

namespace fwk {

class ModelAnim {
  public:
	ModelAnim(CXmlNode, PPose default_pose);
	void saveToXML(XmlNode) const;

	string print() const;
	const string &name() const { return m_name; }
	float length() const { return m_length; }
	// TODO: advanced interpolation support

	static void transToXML(const AffineTrans &trans, const AffineTrans &default_trans, XmlNode);
	static AffineTrans transFromXML(CXmlNode, const AffineTrans &default_trans = AffineTrans());

	PPose animatePose(PPose initial_pose, double anim_time) const;

	void setDefaultPose(PPose);
	vector<Matrix4> animateDefaultPose(double anim_time) const;

  protected:
	string m_name;

	AffineTrans animateChannel(int channel_id, double anim_pos) const;
	void verifyData() const;

	struct Channel {
		Channel() = default;
		Channel(CXmlNode, const AffineTrans &default_trans);
		void saveToXML(XmlNode) const;
		AffineTrans blend(int frame0, int frame1, float t) const;

		// TODO: interpolation information
		AffineTrans trans, default_trans;
		vector<float3> translation_track;
		vector<float3> scaling_track;
		vector<Quat> rotation_track;
		vector<float> time_track;
		string node_name;
	};

	vector<Matrix4> m_default_matrices;
	vector<int> m_default_mapping;

	// TODO: information about whether its looped or not
	vector<Channel> m_channels;
	vector<float> m_shared_time_track;
	vector<string> m_node_names;
	float m_length;
};
}
