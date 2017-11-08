// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/model_anim.h"
#include "fwk/gfx/model_node.h"
#include "fwk/unique_ptr.h"

namespace fwk {

struct MaterialDef {
	MaterialDef(const string &name, FColor diffuse) : name(name), diffuse(diffuse) {}
	MaterialDef(CXmlNode);
	void saveToXML(XmlNode) const;

	string name;
	FColor diffuse;
};

class Model : public immutable_base<Model> {
  public:
	Model() = default;
	Model(Model &&) = default;
	Model(const Model &);
	Model &operator=(Model &&) = default;
	~Model() = default;

	Model(PModelNode, vector<ModelAnim> anims = {}, vector<MaterialDef> material_defs = {});
	static Model loadFromXML(CXmlNode);
	void saveToXML(XmlNode) const;

	// TODO: use this in transformation functions
	// TODO: better name
	/*	void decimate(MeshBuffers &out_buffers, vector<MeshIndices> &out_indices,
					  vector<string> &out_names) {
			out_buffers = move(m_buffers);
			out_indices = move(m_indices);
			out_names = move(m_material_names);
			*this = Mesh();
		}*/

	void join(const string &local_name, const Model &, const string &other_name);

	const ModelNode *findNode(const string &name) const { return m_root->find(name); }
	int findNodeId(const string &name) const;
	const ModelNode *rootNode() const { return m_root.get(); }
	const auto &nodes() const { return m_nodes; }
	const auto &anims() const { return m_anims; }
	const auto &materialDefs() const { return m_material_defs; }

	void printHierarchy() const;

	Matrix4 nodeTrans(const string &name, PPose) const;

	void drawNodes(RenderList &, PPose, IColor node_color, IColor line_color,
				   float node_scale = 1.0f, const Matrix4 &matrix = Matrix4::identity()) const;

	void clearDrawingCache() const;

	const ModelAnim &anim(int anim_id) const { return m_anims[anim_id]; }
	int animCount() const { return m_anims.size(); }

	// Pass -1 to anim_id for bind position
	PPose animatePose(int anim_id, double anim_pos) const;
	PPose defaultPose() const { return m_default_pose; }
	PPose globalPose(PPose local_pose) const;
	PPose meshSkinningPose(PPose global_pose, int node_id) const;
	bool valid(PPose) const;

  protected:
	void updateNodes();

	PModelNode m_root;
	vector<ModelAnim> m_anims;
	vector<MaterialDef> m_material_defs;
	vector<ModelNode *> m_nodes;
	PPose m_default_pose;
};
}
