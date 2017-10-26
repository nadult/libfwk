// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/hash_map.h"
#include "fwk/math/affine_trans.h"
#include "fwk/math/matrix4.h"
#include "fwk/sys/immutable_ptr.h"

namespace fwk {

DEFINE_ENUM(ModelNodeType, generic, mesh, armature, bone, empty);

class ModelNode {
  public:
	using Type = ModelNodeType;

	using Property = pair<string, string>;
	using PropertyMap = HashMap<string, string>;

	ModelNode(const string &name, Type type = Type::generic,
			  const AffineTrans &trans = AffineTrans(), PMesh mesh = PMesh(),
			  vector<Property> props = {});
	ModelNode(const ModelNode &rhs);

	void addChild(PModelNode);
	PModelNode removeChild(const ModelNode *);
	PModelNode clone() const;

	Type type() const { return m_type; }
	const auto &children() const { return m_children; }
	const auto &name() const { return m_name; }
	const auto &properties() const { return m_properties; }
	PropertyMap propertyMap() const;
	const ModelNode *find(const string &name, bool recursive = true) const;

	// TODO: name validation
	void setTrans(const AffineTrans &trans);
	void setName(const string &name) { m_name = name; }
	void setMesh(PMesh mesh) { m_mesh = move(mesh); }
	void setId(int new_id) { m_id = new_id; }

	const auto &localTrans() const { return m_trans; }
	const auto &invLocalTrans() const { return m_inv_trans; }

	Matrix4 globalTrans() const;
	Matrix4 invGlobalTrans() const;

	auto mesh() const { return m_mesh; }
	int id() const { return m_id; }

	const ModelNode *root() const;
	const ModelNode *parent() const { return m_parent; }
	bool isDescendant(const ModelNode *ancestor) const;

	void dfs(vector<ModelNode *> &out);
	bool join(const ModelNode *other, const string &name);

  private:
	vector<PModelNode> m_children;
	vector<Property> m_properties;
	string m_name;
	AffineTrans m_trans;
	Matrix4 m_inv_trans;
	PMesh m_mesh;
	Type m_type;
	int m_id;
	const ModelNode *m_parent;
};
}
