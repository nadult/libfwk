/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <deque>
#include <unordered_map>
#include "fwk_profile.h"

namespace fwk {

MaterialDef::MaterialDef(const XMLNode &node)
	: name(node.attrib("name")), diffuse(node.attrib<float3>("diffuse")) {}

void MaterialDef::saveToXML(XMLNode node) const {
	node.addAttrib("name", node.own(name));
	node.addAttrib("diffuse", float3(diffuse));
}

Model::Model(const XMLNode &node) {
	std::vector<pair<XMLNode, int>> stack;
	vector<XMLNode> root_nodes;
	XMLNode subnode = node.child("node");
	while(subnode) {
		root_nodes.emplace_back(subnode);
		subnode.next();
	}
	std::reverse(begin(root_nodes), end(root_nodes));
	for(const auto &node : root_nodes)
		stack.emplace_back(node, -1);

	while(!stack.empty()) {
		XMLNode xml_node = stack.back().first;
		int parent_id = stack.back().second;
		stack.pop_back();

		int node_id =
			addNode(xml_node.attrib("name"), ModelAnim::transFromXML(xml_node), parent_id);
		Model::Node &new_node = m_nodes[node_id];
		new_node.mesh_id = xml_node.attrib<int>("mesh_id", -1);

		vector<XMLNode> children;
		XMLNode child_node = xml_node.child("node");
		while(child_node) {
			children.push_back(child_node);
			child_node.next();
		}

		std::reverse(begin(children), end(children));
		for(const auto &child : children)
			stack.emplace_back(child, node_id);
	}

	XMLNode mesh_node = node.child("mesh");
	while(mesh_node) {
		if(XMLNode skin_node = mesh_node.child("skin"))
			m_mesh_skins.emplace_back(skin_node);
		else
			m_mesh_skins.emplace_back(MeshSkin());

		m_meshes.emplace_back(mesh_node);
		mesh_node.next();
	}

	XMLNode anim_node = node.child("anim");
	while(anim_node) {
		m_anims.emplace_back(anim_node, *this);
		anim_node.next();
	}

	XMLNode mat_node = node.child("material");
	while(mat_node) {
		m_material_defs.emplace_back(mat_node);
		mat_node.next();
	}

	for(auto &skin : m_mesh_skins)
		skin.attach(*this);

	computeBindMatrices();
	verifyData();
}

Matrix4 Model::computeOffsetMatrix(int node_id) const {
	DASSERT(node_id >= 0 && node_id < (int)m_nodes.size());
	auto bind_pose = Model::finalPose(defaultPose());
	return bind_pose[node_id];
}

void Model::computeBindMatrices() {
	auto def = defaultPose();

	m_bind_matrices = vector<Matrix4>(m_nodes.size(), Matrix4::identity());
	m_inv_bind_matrices.clear();

	for(int n = 0; n < (int)m_nodes.size(); n++) {
		Matrix4 mat = m_nodes[n].trans;
		if(m_nodes[n].parent_id != -1)
			mat = m_bind_matrices[m_nodes[n].parent_id] * mat;
		m_bind_matrices[n] = mat;
		m_inv_bind_matrices.emplace_back(inverse(mat));
	}

	m_bind_scale = float3(1, 1, 1);
	for(const auto &inv_bind : m_inv_bind_matrices) {
		m_bind_scale =
			min(m_bind_scale, float3(length(inv_bind[0].xyz()), length(inv_bind[1].xyz()),
									 length(inv_bind[2].xyz())));
	}
	// TODO: fixing this value fixes junker on new assimp
	m_bind_scale = inv(m_bind_scale);
}

void Model::verifyData() const {
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		const auto &node = m_nodes[n];
		ASSERT(node.parent_id >= -1 && node.parent_id < n);
		ASSERT(node.mesh_id >= -1 && node.mesh_id < (int)m_meshes.size());
	}

	ASSERT(m_meshes.size() == m_mesh_skins.size());
	for(int m = 0; m < (int)m_meshes.size(); m++) {
		const auto &mesh = m_meshes[m];
		const auto &skin = m_mesh_skins[m];
		ASSERT(skin.size() == mesh.vertexCount() || skin.empty());
	}
}

static void saveNodes(const vector<Model::Node> &nodes, int node_id, XMLNode xml_node) {
	const auto &node = nodes[node_id];
	xml_node.addAttrib("name", xml_node.own(node.name));
	if(node.mesh_id != -1)
		xml_node.addAttrib("mesh_id", node.mesh_id);
	ModelAnim::transToXML(node.trans, xml_node);

	for(int child_id : node.children_ids) {
		XMLNode child_node = xml_node.addChild("node");
		saveNodes(nodes, child_id, child_node);
	}
}

void Model::saveToXML(XMLNode xml_node) const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		if(m_nodes[n].parent_id == -1)
			saveNodes(m_nodes, n, xml_node.addChild("node"));

	for(int m = 0; m < (int)m_meshes.size(); m++) {
		const auto &mesh = m_meshes[m];
		const auto &skin = m_mesh_skins[m];

		XMLNode mesh_node = xml_node.addChild("mesh");
		mesh.saveToXML(mesh_node);
		if(!skin.empty())
			skin.saveToXML(mesh_node.addChild("skin"));
	}

	for(const auto &mat : m_material_defs)
		mat.saveToXML(xml_node.addChild("material"));
	for(const auto &anim : m_anims)
		anim.saveToXML(xml_node.addChild("anim"));
}

void Model::assignMaterials(const std::map<string, PMaterial> &map) {
	for(auto &mesh : m_meshes)
		mesh.assignMaterials(map);
}

FBox Model::boundingBox(const ModelPose &pose) const {
	DASSERT(pose.size() == m_nodes.size());

	FBox out = FBox::empty();
	const auto &final_pose = finalPose(pose);
	for(const auto &node : m_nodes)
		if(node.mesh_id != -1) {
			const auto &mesh = m_meshes[node.mesh_id];
			const auto &skin = m_mesh_skins[node.mesh_id];

			FBox bbox;
			if(skin.empty()) {
				bbox = final_pose[node.id] * mesh.boundingBox();
			} else {
				PodArray<float3> positions(mesh.vertexCount());
				animateVertices(node.id, pose, positions);
				bbox = FBox(positions);
			}
			out = out.isEmpty() ? bbox : sum(out, bbox);
		}

	return out;
}

vector<int> Model::findNodes(const vector<string> &names) const {
	vector<int> out;
	for(const auto &name : names)
		out.push_back(findNode(name));
	return out;
}

int Model::findNode(const string &name) const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		if(m_nodes[n].name == name)
			return n;
	return -1;
}

void Model::draw(Renderer &out, const ModelPose &pose, PMaterial material,
				 const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	const auto &final_pose = finalPose(pose);
	for(const auto &node : m_nodes)
		if(node.mesh_id != -1) {
			const auto &mesh = m_meshes[node.mesh_id];
			const auto &skin = m_mesh_skins[node.mesh_id];

			if(skin.empty()) {
				mesh.draw(out, material, final_pose[node.id]);
			} else { Mesh(animateMesh(node.id, pose)).draw(out, material, Matrix4::identity()); }
		}

	out.popViewMatrix();
}

void Model::drawNodes(Renderer &out, const ModelPose &pose, Color node_color, Color line_color,
					  const Matrix4 &matrix) const {
	Mesh bbox_mesh(MakeBBox(), FBox(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f));
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	const auto &final_pose = finalSkinnedPose(pose);
	vector<float3> positions(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++)
		positions[n] = mulPoint(final_pose[n], m_bind_matrices[n][3].xyz());

	auto material = make_shared<Material>(node_color);
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		if(m_inv_bind_matrices[n] != Matrix4::identity())
			bbox_mesh.draw(out, material, translation(positions[n]));
		if(m_nodes[n].parent_id != -1) {
			vector<float3> lines{positions[n], positions[m_nodes[n].parent_id]};
			out.addLines(lines, line_color);
		}
	}

	out.popViewMatrix();
}

void Model::printHierarchy() const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		printf("%d: %s\n", n, m_nodes[n].name.c_str());
}

Mesh Model::toMesh(const ModelPose &pose) const {
	vector<Mesh> meshes;

	const auto &final_pose = finalPose(pose);
	for(const auto &node : m_nodes)
		if(node.mesh_id != -1) {
			const auto &mesh = m_meshes[node.mesh_id];
			const auto &skin = m_mesh_skins[node.mesh_id];

			meshes.emplace_back(skin.empty() ? Mesh::transform(final_pose[node.id], mesh)
											 : animateMesh(node.id, pose));
		}

	return Mesh::merge(meshes);
}

float Model::intersect(const Segment &segment, const ModelPose &pose) const {
	float min_isect = constant::inf;

	const auto &final_pose = finalPose(pose);
	for(const auto &node : m_nodes)
		if(node.mesh_id != -1) {
			const auto &mesh = m_meshes[node.mesh_id];
			const auto &skin = m_mesh_skins[node.mesh_id];

			if(skin.empty()) {
				float isect = mesh.intersect(inverse(final_pose[node.id]) * segment);
				min_isect = min(min_isect, isect);
			} else {
				PodArray<float3> positions(mesh.vertexCount());
				animateVertices(node.id, pose, positions);

				// TODO: optimize
				if(intersection(segment, FBox(positions)) < constant::inf)
					for(const auto &tri : mesh.trisIndices()) {
						float isect =
							intersection(segment, Triangle(positions[tri[0]], positions[tri[1]],
														   positions[tri[2]]));
						min_isect = min(min_isect, isect);
					}
			}
		}

	return min_isect;
}

void Model::animateVertices(int node_id, const ModelPose &pose, Range<float3> out_positions,
							Range<float3> out_normals) const {
	DASSERT(node_id >= 0 && node_id < (int)m_nodes.size());

	const auto &node = m_nodes[node_id];
	DASSERT(node.mesh_id != -1);

	const MeshSkin &skin = m_mesh_skins[node.mesh_id];
	const Mesh &mesh = m_meshes[node.mesh_id];
	updateCounter("Model::animateVertices", 1);
	FWK_PROFILE("Model::animateVertices");
	DASSERT(!skin.empty());

	vector<Matrix4> matrices = finalSkinnedPose(pose);
	Matrix4 offset_matrix = computeOffsetMatrix(node_id);

	for(auto &matrix : matrices)
		matrix = matrix * offset_matrix;

	if(!out_positions.empty()) {
		DASSERT(out_positions.size() == (int)mesh.positions().size());
		std::copy(begin(mesh.positions()), end(mesh.positions()), begin(out_positions));
		skin.animatePositions(out_positions, matrices);
	}
	if(!out_normals.empty()) {
		DASSERT(out_normals.size() == (int)mesh.normals().size());
		std::copy(begin(mesh.normals()), end(mesh.normals()), begin(out_normals));
		skin.animateNormals(out_normals, matrices);
	}
}

Mesh Model::animateMesh(int node_id, const ModelPose &pose) const {
	DASSERT(node_id >= 0 && node_id < (int)m_nodes.size());

	const auto &node = m_nodes[node_id];
	DASSERT(node.mesh_id != -1);
	const auto &mesh = m_meshes[node.mesh_id];

	vector<float3> positions = mesh.positions();
	vector<float2> tex_coords = mesh.texCoords();
	animateVertices(node_id, pose, positions);

	return Mesh({positions, {}, tex_coords}, mesh.indices(), mesh.materials());
}

Matrix4 Model::nodeTrans(const string &name, const ModelPose &pose) const {
	const auto &final_pose = finalPose(pose);
	for(int n = 0; n < (int)m_nodes.size(); n++)
		if(m_nodes[n].name == name)
			return final_pose[n];
	return Matrix4::identity();
}

const vector<Matrix4> &Model::finalPose(const ModelPose &pose) const {
	DASSERT(pose.size() == m_nodes.size());
	if(pose.m_is_dirty) {
		auto &out = pose.m_final;
		out = std::vector<Matrix4>(begin(pose.m_transforms), end(pose.m_transforms));
		for(int n = 0; n < (int)out.size(); n++)
			if(m_nodes[n].parent_id != -1)
				out[n] = out[m_nodes[n].parent_id] * out[n];
		pose.m_is_dirty = false;
	}
	return pose.m_final;
}

const vector<Matrix4> &Model::finalSkinnedPose(const ModelPose &pose) const {
	DASSERT(pose.size() == m_nodes.size());
	if(pose.m_is_skinned_dirty) {
		const auto &normal_poses = Model::finalPose(pose);
		DASSERT(normal_poses.size() == m_nodes.size());

		auto &out = pose.m_skinned_final;
		out.resize(m_nodes.size());
		for(int n = 0; n < (int)out.size(); n++)
			out[n] = scaling(m_bind_scale) * normal_poses[n] * m_inv_bind_matrices[n];

		pose.m_is_skinned_dirty = false;
	}
	return pose.m_skinned_final;
}

ModelPose Model::defaultPose() const {
	vector<AffineTrans> out(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++)
		out[n] = m_nodes[n].trans;
	return ModelPose(std::move(out));
}

ModelPose Model::animatePose(int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < (int)m_anims.size());

	if(anim_id == -1)
		return defaultPose();
	return m_anims[anim_id].animatePose(defaultPose(), anim_pos);
}

int Model::addNode(const string &name, const AffineTrans &trans, int parent_id) {
	DASSERT(parent_id >= -1 && parent_id < (int)m_nodes.size());

	int node_id = (int)m_nodes.size();
	m_nodes.emplace_back(Node{name, trans, node_id, parent_id, -1, vector<int>()});
	if(parent_id != -1)
		m_nodes[parent_id].children_ids.emplace_back(node_id);
	return node_id;
}
}
