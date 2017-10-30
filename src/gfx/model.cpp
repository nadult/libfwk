// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/model.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/material_set.h"
#include "fwk/gfx/mesh.h"
#include "fwk/gfx/pose.h"
#include "fwk/gfx/render_list.h"
#include "fwk/sys/xml.h"
#include <map>

namespace fwk {

MaterialDef::MaterialDef(CXmlNode node)
	: name(node.attrib("name")), diffuse(node.attrib("diffuse", float3(1, 1, 1))) {}

void MaterialDef::saveToXML(XmlNode node) const {
	node.addAttrib("name", node.own(name));
	node.addAttrib("diffuse", diffuse.rgb());
}

namespace {

	PModelNode parseNode(vector<PMesh> &meshes, CXmlNode xml_node) {
		auto name = xml_node.attrib("name");
		auto type = fromString<ModelNodeType>(xml_node.attrib("type", "generic"));
		auto trans = ModelAnim::transFromXML(xml_node);
		auto mesh_id = xml_node.attrib<int>("mesh_id", -1);
		ASSERT(mesh_id >= -1 && mesh_id < meshes.size());

		vector<ModelNode::Property> props;
		auto prop_node = xml_node.child("property");
		while(prop_node) {
			props.emplace_back(prop_node.attrib("name"), prop_node.attrib("value"));
			prop_node.next();
		}

		auto new_node = make_unique<ModelNode>(name, type, trans,
											   mesh_id == -1 ? PMesh() : meshes[mesh_id], props);
		auto child_node = xml_node.child("node");
		while(child_node) {
			new_node->addChild(parseNode(meshes, child_node));
			child_node.next();
		}

		return new_node;
	}

	PPose defaultPose(ModelNode *root) {
		vector<ModelNode *> nodes;
		root->dfs(nodes);

		vector<Matrix4> pose_matrices;
		vector<string> pose_names;

		for(const auto *node : nodes) {
			pose_matrices.emplace_back(node->localTrans());
			pose_names.emplace_back(node->name());
		}
		return Pose(move(pose_matrices), pose_names);
	}
}

Model::Model(PModelNode root, vector<ModelAnim> anims, vector<MaterialDef> material_defs)
	: m_root(move(root)), m_anims(move(anims)), m_material_defs(move(material_defs)) {
	ASSERT(m_root->name() == "" && m_root->localTrans() == AffineTrans() && !m_root->mesh());
	// TODO: verify data
	updateNodes();
}

Model::Model(const Model &rhs) : Model(rhs.m_root->clone(), rhs.m_anims, rhs.m_material_defs) {}

Model Model::loadFromXML(CXmlNode xml_node) {
	DASSERT(xml_node);
	auto mesh_node = xml_node.child("mesh");
	vector<PMesh> meshes;
	while(mesh_node) {
		meshes.emplace_back(make_immutable<Mesh>(mesh_node));
		mesh_node.next();
	}

	PModelNode root = make_unique<ModelNode>("");
	auto subnode = xml_node.child("node");
	while(subnode) {
		root->addChild(parseNode(meshes, subnode));
		subnode.next();
	}

	auto default_pose = fwk::defaultPose(root.get());
	vector<ModelAnim> anims;
	auto anim_node = xml_node.child("anim");
	while(anim_node) {
		anims.emplace_back(anim_node, default_pose);
		anim_node.next();
	}

	vector<MaterialDef> material_defs;
	auto mat_node = xml_node.child("material");
	while(mat_node) {
		material_defs.emplace_back(mat_node);
		mat_node.next();
	}

	return Model(move(root), move(anims), move(material_defs));
}

int Model::findNodeId(const string &name) const {
	const auto *node = findNode(name);
	return node ? node->id() : -1;
}

void Model::updateNodes() {
	m_nodes.clear();
	m_root->dfs(m_nodes);
	for(int n = 0; n < m_nodes.size(); n++)
		m_nodes[n]->setId(n);
	m_default_pose = fwk::defaultPose(m_root.get());
}

static void saveNode(std::map<const Mesh *, int> meshes, const ModelNode *node, XmlNode xml_node) {
	xml_node.addAttrib("name", xml_node.own(node->name()));
	if(node->type() != ModelNodeType::generic)
		xml_node.addAttrib("type", toString(node->type()));
	if(node->mesh())
		xml_node.addAttrib("mesh_id", meshes[node->mesh().get()]);
	ModelAnim::transToXML(node->localTrans(), AffineTrans(), xml_node);

	auto props = node->properties();
	std::sort(begin(props), end(props));
	for(const auto &prop : props) {
		XmlNode xml_prop = xml_node.addChild("property");
		xml_prop.addAttrib("name", xml_prop.own(prop.first));
		xml_prop.addAttrib("value", xml_prop.own(prop.second));
	}

	for(const auto &child : node->children())
		saveNode(meshes, child.get(), xml_node.addChild("node"));
}

void Model::saveToXML(XmlNode xml_node) const {
	std::map<const Mesh *, int> mesh_ids;
	for(const auto *node : m_nodes)
		if(node->mesh() && mesh_ids.find(node->mesh().get()) == mesh_ids.end())
			mesh_ids.emplace(node->mesh().get(), (int)mesh_ids.size());

	for(const auto &node : m_root->children())
		saveNode(mesh_ids, node.get(), xml_node.addChild("node"));

	vector<const Mesh *> meshes(mesh_ids.size(), nullptr);
	for(const auto &pair : mesh_ids)
		meshes[pair.second] = pair.first;
	for(const auto *mesh : meshes)
		mesh->saveToXML(xml_node.addChild("mesh"));

	for(const auto &mat : m_material_defs)
		mat.saveToXML(xml_node.addChild("material"));
	for(const auto &anim : m_anims)
		anim.saveToXML(xml_node.addChild("anim"));
}

void Model::join(const string &local_name, const Model &other, const string &other_name) {
	if(auto *other_node = other.findNode(other_name))
		if(m_root->join(other_node, local_name)) {
			m_material_defs.insert(end(m_material_defs), begin(other.materialDefs()),
								   end(other.materialDefs()));
			// TODO: anims?
			updateNodes();
		}
}

void Model::drawNodes(RenderList &out, PPose pose, IColor node_color, IColor line_color,
					  float node_scale, const Matrix4 &matrix) const {
	// TODO: move this to model viewer
	DASSERT(valid(pose));
	Material node_mat(node_color, MaterialOpt::ignore_depth);
	Material line_mat(line_color, MaterialOpt::ignore_depth);

	Mesh bbox_mesh = Mesh::makeBBox(FBox{{-0.3f, -0.3f, -0.3f}, {0.3f, 0.3f, 0.3f}} * node_scale);
	auto bbox_draw = bbox_mesh.genDrawCalls(MaterialSet{node_mat});

	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	auto global_pose = globalPose(pose);
	auto transforms = global_pose->transforms();

	vector<float3> positions(nodes().size());
	for(int n = 0; n < nodes().size(); n++)
		positions[n] = mulPoint(transforms[n], float3(0, 0, 0));
	if(positions.size() % 2 == 1)
		positions.pop_back();

	for(const auto *node : nodes()) {
		if(node != m_root.get())
			out.add(bbox_draw, translation(positions[node->id()]));
		if(node->parent() && node->parent() != m_nodes.front()) {
			float3 line[2] = {positions[node->id()], positions[node->parent()->id()]};
			out.lines().add(line, line_mat);
		}
	}

	out.popViewMatrix();
}

void Model::printHierarchy() const {
	for(const auto *node : nodes())
		printf("%d: %s\n", node->id(), node->name().c_str());
}

Matrix4 Model::nodeTrans(const string &name, PPose pose) const {
	if(!pose)
		pose = m_default_pose;

	auto global_pose = globalPose(pose);
	if(const auto *node = findNode(name))
		return global_pose->transforms()[node->id()];
	return Matrix4::identity();
}

PPose Model::globalPose(PPose pose) const {
	DASSERT(valid(pose));
	vector<Matrix4> out = pose->transforms();
	for(int n = 0; n < out.size(); n++)
		if(nodes()[n]->parent())
			out[n] = out[nodes()[n]->parent()->id()] * out[n];
	return make_immutable<Pose>(move(out), pose->nameMap());
}

PPose Model::meshSkinningPose(PPose global_pose, int node_id) const {
	DASSERT(valid(global_pose));
	DASSERT(node_id >= 0 && node_id < m_nodes.size());

	Matrix4 pre = inverse(m_nodes[node_id]->globalTrans());
	Matrix4 post = m_nodes[node_id]->globalTrans();

	vector<Matrix4> out = global_pose->transforms();
	for(int n = 0; n < out.size(); n++)
		out[n] = pre * out[n] * m_nodes[n]->invGlobalTrans() * post;
	return make_immutable<Pose>(move(out), global_pose->nameMap());
}

PPose Model::animatePose(int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < m_anims.size());

	if(anim_id == -1)
		return defaultPose();
	return m_anims[anim_id].animatePose(defaultPose(), anim_pos);
}

bool Model::valid(PPose pose) const {
	// TODO: keep weak_ptr to model inside pose?
	return pose && pose->nameMap() == m_default_pose->nameMap();
}
}
