/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <deque>
#include <unordered_map>
#include "fwk_cache.h"

namespace fwk {

MaterialDef::MaterialDef(const XMLNode &node)
	: name(node.attrib("name")), diffuse(node.attrib("diffuse", float3(1, 1, 1))) {}

void MaterialDef::saveToXML(XMLNode node) const {
	node.addAttrib("name", node.own(name));
	node.addAttrib("diffuse", float3(diffuse));
}

auto makeNameMap(const vector<string> &names) {
	vector<pair<string, int>> out(names.size());
	for(int n = 0; n < (int)names.size(); n++)
		out[n] = make_pair(names[n], n);
	std::sort(begin(out), end(out));
	return out;
}

Pose::Pose(vector<Matrix4> transforms, NameMap name_map)
	: m_name_map(std::move(name_map)), m_transforms(std::move(transforms)) {
	DASSERT(m_transforms.size() == m_name_map.size());
}
Pose::Pose(vector<Matrix4> transforms, const vector<string> &names)
	: Pose(std::move(transforms), makeNameMap(names)) {}

vector<int> Pose::mapNames(const vector<string> &names) const {
	auto dst_map = makeNameMap(names);
	int src_index = 0;
	vector<int> out(names.size());

	for(int n = 0; n < (int)dst_map.size(); n++) {
		const string &name = dst_map[n].first;
		while(m_name_map[src_index].first != name) {
			src_index++;
			if(src_index == (int)m_name_map.size())
				THROW("Cannot find node in pose: %s", name.c_str());
		}

		out[dst_map[n].second] = m_name_map[src_index++].second;
	}
	return out;
}

vector<Matrix4> Pose::mapTransforms(const vector<int> &mapping) const {
	vector<Matrix4> out;
	out.reserve(mapping.size());
	for(auto it : mapping) {
		DASSERT(it >= 0 && it < (int)m_transforms.size());
		out.emplace_back(m_transforms[it]);
	}
	return out;
}

namespace {

	PModelNode parseNode(vector<PMesh> &meshes, XMLNode xml_node) {
		auto name = xml_node.attrib("name");
		auto type = fromString<ModelNodeType>(xml_node.attrib("type", "generic"));
		auto trans = ModelAnim::transFromXML(xml_node);
		auto mesh_id = xml_node.attrib<int>("mesh_id", -1);
		ASSERT(mesh_id >= -1 && mesh_id < (int)meshes.size());

		vector<ModelNode::Property> props;
		XMLNode prop_node = xml_node.child("property");
		while(prop_node) {
			props.emplace_back(
				ModelNode::Property{prop_node.attrib("name"), prop_node.attrib("value")});
			prop_node.next();
		}

		auto new_node = make_unique<ModelNode>(name, type, trans,
											   mesh_id == -1 ? PMesh() : meshes[mesh_id], props);
		XMLNode child_node = xml_node.child("node");
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
		return Pose(std::move(pose_matrices), pose_names);
	}
}

Model::Model(PModelNode root, vector<ModelAnim> anims, vector<MaterialDef> material_defs)
	: m_root(std::move(root)), m_anims(std::move(anims)),
	  m_material_defs(std::move(material_defs)) {
	ASSERT(m_root->name() == "" && m_root->localTrans() == AffineTrans() && !m_root->mesh());
	// TODO: verify data
	updateNodes();
}

Model::Model(const Model &rhs) : Model(rhs.m_root->clone(), rhs.m_anims, rhs.m_material_defs) {}

Model Model::loadFromXML(const XMLNode &xml_node) {
	XMLNode mesh_node = xml_node.child("mesh");
	vector<PMesh> meshes;
	while(mesh_node) {
		meshes.emplace_back(make_immutable<Mesh>(mesh_node));
		mesh_node.next();
	}

	PModelNode root = make_unique<ModelNode>("");
	XMLNode subnode = xml_node.child("node");
	while(subnode) {
		root->addChild(parseNode(meshes, subnode));
		subnode.next();
	}

	auto default_pose = fwk::defaultPose(root.get());
	vector<ModelAnim> anims;
	XMLNode anim_node = xml_node.child("anim");
	while(anim_node) {
		anims.emplace_back(anim_node, default_pose);
		anim_node.next();
	}

	vector<MaterialDef> material_defs;
	XMLNode mat_node = xml_node.child("material");
	while(mat_node) {
		material_defs.emplace_back(mat_node);
		mat_node.next();
	}

	return Model(std::move(root), std::move(anims), std::move(material_defs));
}

int Model::findNodeId(const string &name) const {
	const auto *node = findNode(name);
	return node ? node->id() : -1;
}

void Model::updateNodes() {
	m_nodes.clear();
	m_root->dfs(m_nodes);
	for(int n = 0; n < (int)m_nodes.size(); n++)
		m_nodes[n]->setId(n);
	m_default_pose = fwk::defaultPose(m_root.get());
}

static void saveNode(std::map<const Mesh *, int> meshes, const ModelNode *node, XMLNode xml_node) {
	xml_node.addAttrib("name", xml_node.own(node->name()));
	if(node->type() != ModelNodeType::generic)
		xml_node.addAttrib("type", toString(node->type()));
	if(node->mesh())
		xml_node.addAttrib("mesh_id", meshes[node->mesh().get()]);
	ModelAnim::transToXML(node->localTrans(), AffineTrans(), xml_node);

	auto props = node->properties();
	std::sort(begin(props), end(props));
	for(const auto &prop : props) {
		XMLNode xml_prop = xml_node.addChild("property");
		xml_prop.addAttrib("name", xml_prop.own(prop.name));
		xml_prop.addAttrib("value", xml_prop.own(prop.value));
	}

	for(const auto &child : node->children())
		saveNode(meshes, child.get(), xml_node.addChild("node"));
}

void Model::saveToXML(XMLNode xml_node) const {
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

void Model::drawNodes(RenderList &out, PPose pose, Color node_color, Color line_color,
					  float node_scale, const Matrix4 &matrix) const {
	// TODO: move this to model viewer
	DASSERT(valid(pose));
	auto node_mat = make_immutable<Material>(node_color, Material::flag_ignore_depth);
	auto line_mat = make_immutable<Material>(line_color, Material::flag_ignore_depth);

	Mesh bbox_mesh = Mesh::makeBBox(FBox{-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f} * node_scale);
	auto bbox_draw = bbox_mesh.genDrawCalls(node_mat);

	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	auto global_pose = globalPose(pose);
	auto transforms = global_pose->transforms();

	vector<float3> positions(nodes().size());
	for(int n = 0; n < (int)nodes().size(); n++)
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
	for(int n = 0; n < (int)out.size(); n++)
		if(nodes()[n]->parent())
			out[n] = out[nodes()[n]->parent()->id()] * out[n];
	return make_immutable<Pose>(std::move(out), pose->nameMap());
}

PPose Model::meshSkinningPose(PPose global_pose, int node_id) const {
	DASSERT(valid(global_pose));
	DASSERT(node_id >= 0 && node_id < (int)m_nodes.size());

	Matrix4 pre = inverse(m_nodes[node_id]->globalTrans());
	Matrix4 post = m_nodes[node_id]->globalTrans();

	vector<Matrix4> out = global_pose->transforms();
	for(int n = 0; n < (int)out.size(); n++)
		out[n] = pre * out[n] * m_nodes[n]->invGlobalTrans() * post;
	return make_immutable<Pose>(std::move(out), global_pose->nameMap());
}

PPose Model::animatePose(int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < (int)m_anims.size());

	if(anim_id == -1)
		return defaultPose();
	return m_anims[anim_id].animatePose(defaultPose(), anim_pos);
}

bool Model::valid(PPose pose) const {
	// TODO: keep weak_ptr to model inside pose?
	return pose && pose->nameMap() == m_default_pose->nameMap();
}
}
