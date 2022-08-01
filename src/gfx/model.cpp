// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/model.h"

#include "fwk/gfx/drawing.h"
#include "fwk/gfx/line_buffer.h"
#include "fwk/gfx/mesh.h"
#include "fwk/gfx/pose.h"
#include "fwk/gfx/triangle_buffer.h"
#include "fwk/index_range.h"
#include "fwk/io/xml.h"
#include "fwk/sys/assert.h"

namespace fwk {

MaterialDef::MaterialDef(CXmlNode node)
	: name(node("name")), diffuse(node("diffuse", float3(1, 1, 1))) {}

void MaterialDef::saveToXML(XmlNode node) const {
	node("name") = node.own(name);
	node("diffuse") = diffuse.rgb();
}

static Pose defaultPose(vector<int> dfs_ids, CSpan<ModelNode> nodes) {
	vector<Matrix4> pose_matrices;
	vector<string> pose_names;

	for(auto id : dfs_ids) {
		auto &node = nodes[id];
		pose_matrices.emplace_back(node.trans);
		pose_names.emplace_back(node.name);
	}
	return {move(pose_matrices), pose_names};
}

static void dfs(CSpan<ModelNode> nodes, int node_id, vector<int> &out) {
	out.emplace_back(node_id);
	for(auto child_id : nodes[node_id].children_ids)
		dfs(nodes, child_id, out);
}

Model::Model(vector<ModelNode> nodes, vector<Mesh> meshes, vector<ModelAnim> anims,
			 vector<MaterialDef> material_defs)
	: m_nodes(move(nodes)), m_meshes(meshes), m_anims(move(anims)),
	  m_material_defs(move(material_defs)) {
	if(auto *root = rootNode())
		ASSERT(root->name == "" && root->trans == AffineTrans() && root->mesh_id == -1);

	m_default_pose = fwk::defaultPose(dfs(0), m_nodes);

	// TODO: verify data
	for(auto &anim : m_anims)
		anim.setDefaultPose(m_default_pose);
}

FWK_COPYABLE_CLASS_IMPL(Model);

Ex<void> parseNodes(vector<ModelNode> &out, int node_id, int num_meshes, CXmlNode xml_node) {
	xml_node = xml_node.child("node");
	while(xml_node) {
		ModelNode new_node;
		new_node.name = xml_node("name");
		new_node.type = xml_node("type", ModelNodeType::generic);
		auto trans = ModelAnim::transFromXML(xml_node);
		EX_CATCH();
		new_node.setTrans(trans);

		new_node.mesh_id = xml_node("mesh_id", -1);
		EXPECT(new_node.mesh_id >= -1 && new_node.mesh_id < num_meshes);

		auto prop_node = xml_node.child("property");
		while(prop_node) {
			new_node.props.emplace_back(prop_node.attrib("name"), prop_node.attrib("value"));
			prop_node.next();
		}

		EX_CATCH();
		int sub_node_id = out.size();
		new_node.parent_id = node_id;
		new_node.id = sub_node_id;
		out.emplace_back(move(new_node));
		out[node_id].children_ids.emplace_back(sub_node_id);
		EXPECT(parseNodes(out, sub_node_id, num_meshes, xml_node));
		xml_node.next();
	}

	return {};
}

Ex<Model> Model::load(CXmlNode xml_node) {
	DASSERT(xml_node);

	auto mesh_node = xml_node.child("mesh");
	vector<Mesh> meshes;
	while(mesh_node) {
		meshes.emplace_back(EX_PASS(Mesh::load(mesh_node)));
		mesh_node.next();
	}

	vector<ModelNode> nodes;
	nodes.emplace_back();
	EXPECT(parseNodes(nodes, 0, meshes.size(), xml_node));

	vector<int> dfs_ids;
	fwk::dfs(nodes, 0, dfs_ids);

	auto default_pose = fwk::defaultPose(dfs_ids, nodes);
	vector<ModelAnim> anims;
	auto anim_node = xml_node.child("anim");
	while(anim_node) {
		anims.emplace_back(EX_PASS(ModelAnim::load(anim_node, default_pose)));
		anim_node.next();
	}

	vector<MaterialDef> material_defs;
	auto mat_node = xml_node.child("material");
	while(mat_node) {
		material_defs.emplace_back(mat_node);
		mat_node.next();
	}

	// TODO: make sure that errors are handled properly here
	return Model(move(nodes), move(meshes), move(anims), move(material_defs));
}

Ex<Model> Model::load(ZStr file_name) {
	auto doc = EX_PASS(XmlDocument::load(file_name));
	XmlOnFailGuard guard(doc);

	XmlNode child = doc.child();
	EXPECT(child);
	return load(child);
}

vector<int> Model::dfs(int root_id) const {
	vector<int> out;
	fwk::dfs(m_nodes, root_id, out);
	return out;
}

const ModelNode *Model::findNode(Str name) const { return node(findNodeId(name)); }

int Model::findNodeId(Str name) const {
	for(int n : intRange(m_nodes))
		if(m_nodes[n].name == name)
			return n;
	return -1;
}

static void saveNode(CSpan<ModelNode> nodes, int node_id, XmlNode xml_node) {
	auto &node = nodes[node_id];
	xml_node("name") = xml_node.own(node.name);
	if(node.type != ModelNodeType::generic)
		xml_node("type") = toString(node.type);
	xml_node("mesh_id") = node.mesh_id;
	ModelAnim::transToXML(node.trans, AffineTrans(), xml_node);

	auto props = node.props;
	std::sort(begin(props), end(props));
	for(const auto &prop : props) {
		XmlNode xml_prop = xml_node.addChild("property");
		xml_prop("name") = xml_prop.own(prop.first);
		xml_prop("value") = xml_prop.own(prop.second);
	}

	for(int child_id : node.children_ids)
		saveNode(nodes, child_id, xml_node.addChild("node"));
}

void Model::save(XmlNode xml_node) const {
	if(m_nodes)
		for(int nid : m_nodes[0].children_ids)
			saveNode(m_nodes, nid, xml_node.addChild("node"));

	for(const auto &mesh : m_meshes)
		mesh.saveToXML(xml_node.addChild("mesh"));
	for(const auto &mat : m_material_defs)
		mat.saveToXML(xml_node.addChild("material"));
	for(const auto &anim : m_anims)
		anim.save(xml_node.addChild("anim"));
}

/*void Model::drawNodes(TriangleBuffer &tris, LineBuffer &lines, const Pose &pose, IColor node_color,
					  IColor line_color, float node_scale) const {
	// TODO: output to Visualizer3?
	// TODO: move this to model viewer
	DASSERT(valid(pose));
	// TODO: ignore depth
	SimpleMaterial node_mat(node_color, SimpleMa::ignore_depth);
	SimpleMaterial line_mat(line_color, MaterialOpt::ignore_depth);

	tris.setMaterial(node_mat);
	lines.setMaterial(line_mat);

	FBox bbox = FBox(float3(-0.3f), float3(0.3f)) * node_scale;

	auto global_pose = globalPose(pose);
	auto transforms = global_pose.transforms();

	vector<float3> positions(m_nodes.size());
	for(int n : intRange(m_nodes))
		positions[n] = mulPoint(transforms[n], float3(0, 0, 0));

	for(auto &node : m_nodes) {
		if(&node != rootNode())
			tris(bbox, translation(positions[node.id]));
		if(node.parent_id > 0) {
			float3 line[2] = {positions[node.id], positions[node.parent_id]};
			lines(line);
		}
	}
}*/

void Model::printHierarchy() const {
	for(auto &node : m_nodes)
		print("%: %\n", node.id, node.name);
}

Matrix4 Model::nodeTrans(const string &name, const Pose &pose) const {
	auto global_pose = globalPose(pose);
	if(const auto *node = findNode(name))
		return global_pose.transforms()[node->id];
	return Matrix4::identity();
}

Pose Model::globalPose(vector<Matrix4> out) const {
	DASSERT_EQ(out.size(), m_nodes.size());
	for(int n : intRange(m_nodes))
		if(m_nodes[n].parent_id != -1)
			out[n] = out[m_nodes[n].parent_id] * out[n];
	return {move(out), m_default_pose.nameMap()};
}

Pose Model::globalPose(const Pose &pose) const {
	DASSERT(valid(pose));
	vector<Matrix4> out = pose.transforms();
	for(int n : intRange(m_nodes))
		if(m_nodes[n].parent_id != -1)
			out[n] = out[m_nodes[n].parent_id] * out[n];
	return {move(out), pose.nameMap()};
}

Matrix4 Model::globalTrans(int node_id) const {
	auto &node = m_nodes[node_id];
	return node.parent_id == -1 ? node.trans : globalTrans(node.parent_id) * node.trans;
}

Matrix4 Model::invGlobalTrans(int node_id) const {
	auto &node = m_nodes[node_id];
	return node.parent_id == -1 ? node.inv_trans : node.inv_trans * invGlobalTrans(node.parent_id);
}

Pose Model::meshSkinningPose(const Pose &global_pose, int node_id) const {
	DASSERT(valid(global_pose));
	DASSERT(node_id >= 0 && node_id < m_nodes.size());

	auto fwd_trans = globalTrans(node_id);
	Matrix4 pre = inverseOrZero(fwd_trans);
	Matrix4 post = fwd_trans;

	vector<Matrix4> out = global_pose.transforms();
	for(int n : intRange(out))
		out[n] = pre * out[n] * invGlobalTrans(n) * post;
	return {move(out), global_pose.nameMap()};
}

vector<Matrix4> Model::animatePoseFast(int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < m_anims.size());
	if(anim_id == -1)
		return m_default_pose.transforms();
	return m_anims[anim_id].animateDefaultPose(anim_pos);
}

Pose Model::animatePose(int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < m_anims.size());

	if(anim_id == -1)
		return defaultPose();
	return m_anims[anim_id].animatePose(defaultPose(), anim_pos);
}

bool Model::valid(const Pose &pose) const { return pose.nameMap() == m_default_pose.nameMap(); }

Model Model::split(int node_id) const {
	DASSERT(node_id >= 0 && node_id < m_nodes.size());
	vector<int> new_node_ids(m_nodes.size(), -1), new_mesh_ids(m_meshes.size(), -1);

	vector<ModelNode> new_nodes;
	vector<Mesh> new_meshes;

	new_nodes.emplace_back(ModelNode());
	new_nodes[0].children_ids.emplace_back(node_id);

	for(int nid : dfs(node_id)) {
		auto &node = m_nodes[nid];
		new_node_ids[nid] = new_nodes.size();
		if(node.mesh_id != -1 && new_mesh_ids[node.mesh_id] == -1) {
			new_mesh_ids[node.mesh_id] = new_meshes.size();
			new_meshes.emplace_back(m_meshes[node.mesh_id]);
		}
		new_nodes.emplace_back(node);
	}

	for(int nid : intRange(new_nodes)) {
		auto &node = new_nodes[nid];
		node.id = nid;
		if(node.parent_id != -1)
			node.parent_id = new_node_ids[node.parent_id];
		for(auto &cid : node.children_ids)
			cid = new_node_ids[cid];
		if(node.mesh_id != -1)
			node.mesh_id = new_mesh_ids[node.mesh_id];
	}

	new_nodes[1].trans.translation = {};

	// TODO: split ModelAnims
	return {move(new_nodes), move(new_meshes), m_anims, m_material_defs};
}
}
