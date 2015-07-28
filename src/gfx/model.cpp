/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <deque>
#include <unordered_map>
#include "fwk_profile.h"
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
		auto trans = ModelAnim::transFromXML(xml_node);
		auto mesh_id = xml_node.attrib<int>("mesh_id", -1);
		ASSERT(mesh_id >= -1 && mesh_id < (int)meshes.size());

		auto new_node =
			make_unique<ModelNode>(name, trans, mesh_id == -1 ? PMesh() : meshes[mesh_id]);
		XMLNode child_node = xml_node.child("node");
		while(child_node) {
			new_node->addChild(parseNode(meshes, child_node));
			child_node.next();
		}

		return new_node;
	}

	PModelNode parseTree(XMLNode xml_node) {
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
		return root;
	}

	vector<ModelAnim> parseAnims(XMLNode xml_node) {
		vector<ModelAnim> out;
		XMLNode anim_node = xml_node.child("anim");
		while(anim_node) {
			out.emplace_back(anim_node);
			anim_node.next();
		}
		return out;
	}

	vector<MaterialDef> parseMaterials(XMLNode xml_node) {
		vector<MaterialDef> out;
		XMLNode mat_node = xml_node.child("material");
		while(mat_node) {
			out.emplace_back(mat_node);
			mat_node.next();
		}

		return out;
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

Model::Model(const XMLNode &node)
	: Model(parseTree(node), parseAnims(node), parseMaterials(node)) {}

int Model::findNodeId(const string &name) const {
	const auto *node = findNode(name);
	return node ? node->id() : -1;
}

void Model::updateNodes() {
	m_nodes.clear();
	m_root->dfs(m_nodes);

	for(int n = 0; n < (int)m_nodes.size(); n++)
		m_nodes[n]->setId(n);

	auto def = defaultPose();

	m_inv_bind_matrices.clear();
	for(const auto *node : nodes())
		m_inv_bind_matrices.emplace_back(inverse(node->globalTrans()));

	// TODO: is this really needed?
	m_bind_scale = float3(1, 1, 1);
	for(const auto &inv_bind : m_inv_bind_matrices) {
		m_bind_scale =
			min(m_bind_scale, float3(length(inv_bind[0].xyz()), length(inv_bind[1].xyz()),
									 length(inv_bind[2].xyz())));
	}
	// TODO: fixing this value fixes junker on new assimp
	m_bind_scale = inv(m_bind_scale);

	vector<Matrix4> pose_matrices;
	vector<string> pose_names;

	for(const auto *node : m_nodes) {
		pose_matrices.emplace_back(node->localTrans());
		pose_names.emplace_back(node->name());
	}
	m_default_pose = Pose(std::move(pose_matrices), pose_names);
}

static void saveNode(std::map<const Mesh *, int> meshes, const ModelNode *node, XMLNode xml_node) {
	xml_node.addAttrib("name", xml_node.own(node->name()));
	if(node->mesh())
		xml_node.addAttrib("mesh_id", meshes[node->mesh().get()]);
	ModelAnim::transToXML(node->localTrans(), xml_node);

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

	for(const auto &pair : mesh_ids)
		pair.first->saveToXML(xml_node.addChild("mesh"));

	for(const auto &mat : m_material_defs)
		mat.saveToXML(xml_node.addChild("material"));
	for(const auto &anim : m_anims)
		anim.saveToXML(xml_node.addChild("anim"));
}

FBox Model::boundingBox(PPose pose) const {
	DASSERT(isValidPose(pose));

	FBox out = FBox::empty();
	auto anim_data = animatedData(pose);
	for(auto pair : anim_data->meshes) {
		FBox bbox = pair.first * pair.second->boundingBox();
		out = out.isEmpty() ? bbox : sum(out, bbox);
	}

	return out;
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

void Model::draw(Renderer &out, PPose pose, const MaterialSet &materials,
				 const Matrix4 &matrix) const {
	DASSERT(isValidPose(pose));
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	auto anim_data = animatedData(pose);
	for(auto pair : anim_data->meshes)
		pair.second->draw(out, materials, pair.first);

	out.popViewMatrix();
}

void Model::drawNodes(Renderer &out, PPose pose, Color node_color, Color line_color,
					  const Matrix4 &matrix) const {
	DASSERT(isValidPose(pose));
	Mesh bbox_mesh = Mesh::makeBBox({-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f});
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	auto global_pose = globalPose(pose);
	auto transforms = global_pose->transforms();

	vector<float3> positions(nodes().size());
	for(int n = 0; n < (int)nodes().size(); n++)
		positions[n] = mulPoint(transforms[n], float3(0, 0, 0));

	auto material = make_immutable<Material>(node_color);
	for(const auto *node : nodes()) {
		if(m_inv_bind_matrices[node->id()] != Matrix4::identity())
			bbox_mesh.draw(out, material, translation(positions[node->id()]));
		if(node->parent()) {
			vector<float3> lines{positions[node->id()], positions[node->parent()->id()]};
			out.addLines(lines, line_color);
		}
	}

	out.popViewMatrix();
}

void Model::printHierarchy() const {
	for(const auto *node : nodes())
		printf("%d: %s\n", node->id(), node->name().c_str());
}

Mesh Model::toMesh(PPose pose) const {
	DASSERT(isValidPose(pose));
	vector<Mesh> meshes;

	auto global_pose = globalPose(pose);
	const auto &transforms = global_pose->transforms();

	for(const auto *node : nodes())
		if(node->mesh()) {
			auto mesh = node->mesh()->animate(meshSkinningPose(global_pose, node->id()));
			meshes.emplace_back(Mesh::transform(transforms[node->id()], mesh));
		}

	return Mesh::merge(meshes);
}

float Model::intersect(const Segment &segment, PPose pose) const {
	DASSERT(isValidPose(pose));
	float min_isect = constant::inf;

	auto anim_data = animatedData(pose);
	for(auto pair : anim_data->meshes) {
		float isect = pair.second->intersect(inverse(pair.first) * segment);
		min_isect = min(min_isect, isect);
	}

	return min_isect;
}

Matrix4 Model::nodeTrans(const string &name, PPose pose) const {
	DASSERT(isValidPose(pose));
	auto global_pose = globalPose(pose);
	if(const auto *node = findNode(name))
		return global_pose->transforms()[node->id()];
	return Matrix4::identity();
}

PPose Model::globalPose(PPose pose) const {
	DASSERT(isValidPose(pose));
	vector<Matrix4> out = pose->transforms();
	for(int n = 0; n < (int)out.size(); n++)
		if(nodes()[n]->parent())
			out[n] = out[nodes()[n]->parent()->id()] * out[n];
	return make_immutable<Pose>(std::move(out), pose->nameMap());
}

PPose Model::meshSkinningPose(PPose global_pose, int node_id) const {
	DASSERT(isValidPose(global_pose));
	FWK_PROFILE("Model::meshSkinningPose");
	DASSERT(node_id >= 0 && node_id < (int)m_nodes.size());

	Matrix4 pre = m_inv_bind_matrices[node_id] * scaling(m_bind_scale);
	Matrix4 post = m_nodes[node_id]->globalTrans();

	vector<Matrix4> out = global_pose->transforms();
	for(int n = 0; n < (int)out.size(); n++)
		out[n] = pre * out[n] * m_inv_bind_matrices[n] * post;
	return make_immutable<Pose>(std::move(out), global_pose->nameMap());
}

PPose Model::animatePose(int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < (int)m_anims.size());

	if(anim_id == -1)
		return defaultPose();
	return m_anims[anim_id].animatePose(defaultPose(), anim_pos);
}

bool Model::isValidPose(PPose pose) const {
	// TODO: keep weak_ptr to model inside pose?
	return pose && pose->nameMap() == m_default_pose->nameMap();
}

immutable_ptr<Model::AnimatedData> Model::animatedData(PPose pose) const {
	auto &cache = TCache<AnimatedData, PModel, PPose>::g_cache;
	auto key = cache.makeKey(get_immutable_ptr(), pose);

	if(auto data = cache.access(key))
		return data;

	AnimatedData new_data;
	new_data.global_pose = globalPose(pose);
	const auto &transforms = new_data.global_pose->transforms();

	for(const auto *node : nodes())
		if(node->mesh()) {
			PMesh mesh;
			if(node->mesh()->hasSkin()) {
				auto skinning_pose = meshSkinningPose(new_data.global_pose, node->id());
				mesh = PMesh(node->mesh()->animate(skinning_pose));
			} else { mesh = node->mesh(); }

			new_data.meshes.emplace_back(transforms[node->id()], mesh);
		}

	auto data_ptr = make_immutable<AnimatedData>(std::move(new_data));
	cache.add(key, data_ptr);
	return data_ptr;
}

template <class Value, class... Keys> TCache<Value, Keys...> TCache<Value, Keys...>::g_cache;
}
