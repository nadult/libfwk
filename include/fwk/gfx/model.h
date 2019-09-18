// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/model_anim.h"
#include "fwk/gfx/model_node.h"
#include "fwk/gfx/pose.h"

namespace fwk {

struct MaterialDef {
	MaterialDef(const string &name, FColor diffuse) : name(name), diffuse(diffuse) {}
	MaterialDef(CXmlNode);
	void saveToXML(XmlNode) const;

	string name;
	FColor diffuse;
};

class Model {
  public:
	FWK_COPYABLE_CLASS(Model);

	Model(vector<ModelNode> = {}, vector<Mesh> = {}, vector<ModelAnim> = {},
		  vector<MaterialDef> = {});
	static Ex<Model> load(CXmlNode);
	static Ex<Model> load(ZStr file_name);
	void save(XmlNode) const;

	const ModelNode *findNode(Str) const;
	int findNodeId(Str) const;

	const ModelNode *node(int id) const { return id == -1 ? nullptr : &m_nodes[id]; }
	const ModelNode *rootNode() const { return m_nodes ? &m_nodes[0] : nullptr; }

	const auto &meshes() const { return m_meshes; }
	const auto &nodes() const { return m_nodes; }
	const auto &anims() const { return m_anims; }
	const auto &materialDefs() const { return m_material_defs; }

	vector<int> dfs(int root_id = 0) const;

	void printHierarchy() const;

	Matrix4 nodeTrans(const string &name, const Pose &) const;

	void drawNodes(TriangleBuffer &, LineBuffer &, const Pose &, IColor node_color,
				   IColor line_color, float node_scale = 1.0f) const;

	void clearDrawingCache() const;

	const ModelAnim &anim(int anim_id) const { return m_anims[anim_id]; }
	int animCount() const { return m_anims.size(); }

	// Pass -1 to anim_id for bind position
	Pose animatePose(int anim_id, double anim_pos) const;
	vector<Matrix4> animatePoseFast(int anim_id, double anim_pos) const;

	Matrix4 globalTrans(int node_id) const;
	Matrix4 invGlobalTrans(int node_id) const;

	const Pose &defaultPose() const { return m_default_pose; }
	Pose globalPose(vector<Matrix4>) const;
	Pose globalPose(const Pose &local_pose) const;
	Pose meshSkinningPose(const Pose &global_pose, int node_id) const;
	bool valid(const Pose &) const;

	const Mesh *mesh(int id) const { return id == -1 ? nullptr : &m_meshes[id]; }

	Model split(int node_id) const;

  protected:
	vector<ModelNode> m_nodes;
	vector<Mesh> m_meshes;
	vector<ModelAnim> m_anims;
	vector<MaterialDef> m_material_defs;
	Pose m_default_pose;
};
}
