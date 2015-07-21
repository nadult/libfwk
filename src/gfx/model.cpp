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
	: name(node.attrib("name")), diffuse(node.attrib("diffuse", float3(1, 1, 1))) {}

void MaterialDef::saveToXML(XMLNode node) const {
	node.addAttrib("name", node.own(name));
	node.addAttrib("diffuse", float3(diffuse));
}

static void parseTree(ModelTree &tree, const ModelNode *parent, XMLNode xml_node) {
	auto *new_node =
		tree.addNode(parent, xml_node.attrib("name"), ModelAnim::transFromXML(xml_node),
					 xml_node.attrib<int>("mesh_id", -1));
	ASSERT(new_node && "nodes should have distinct names");

	XMLNode child_node = xml_node.child("node");
	while(child_node) {
		parseTree(tree, new_node, child_node);
		child_node.next();
	}
}

Model::Model(const XMLNode &node) {
	std::vector<pair<XMLNode, const ModelNode *>> stack;
	XMLNode subnode = node.child("node");
	while(subnode) {
		parseTree(m_tree, m_tree.root(), subnode);
		subnode.next();
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
	DASSERT(node_id >= 0 && node_id < (int)nodes().size());
	auto bind_pose = Model::finalPose(defaultPose());
	return bind_pose[node_id];
}

void Model::computeBindMatrices() {
	auto def = defaultPose();

	m_bind_matrices = vector<Matrix4>(nodes().size(), Matrix4::identity());
	m_inv_bind_matrices.clear();

	for(const auto *node : nodes()) {
		Matrix4 mat = node->globalTrans();
		m_bind_matrices.emplace_back(mat);
		m_inv_bind_matrices.emplace_back(inverse(mat));
	}

	// TODO: is this really needed?
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
	// TODO: verify mesh indices
	ASSERT(m_meshes.size() == m_mesh_skins.size());
	for(int m = 0; m < (int)m_meshes.size(); m++) {
		const auto &mesh = m_meshes[m];
		const auto &skin = m_mesh_skins[m];
		ASSERT(skin.size() == mesh.vertexCount() || skin.empty());
	}
}

static void saveNode(const ModelTree &tree, const ModelNode *node, XMLNode xml_node) {
	xml_node.addAttrib("name", xml_node.own(node->name()));
	if(node->meshId() != -1)
		xml_node.addAttrib("mesh_id", node->meshId());
	ModelAnim::transToXML(node->localTrans(), xml_node);

	for(const auto &child : node->children())
		saveNode(tree, child.get(), xml_node.addChild("node"));
}

void Model::saveToXML(XMLNode xml_node) const {
	for(const auto &node : m_tree.root()->children())
		saveNode(m_tree, node.get(), xml_node.addChild("node"));

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
	DASSERT(pose.size() == nodes().size());

	FBox out = FBox::empty();
	const auto &final_pose = finalPose(pose);
	for(const auto &node : nodes())
		if(node->meshId() != -1) {
			const auto &mesh = m_meshes[node->meshId()];
			const auto &skin = m_mesh_skins[node->meshId()];

			FBox bbox;
			if(skin.empty()) {
				bbox = final_pose[node->id()] * mesh.boundingBox();
			} else {
				PodArray<float3> positions(mesh.vertexCount());
				animateVertices(node->id(), pose, positions);
				bbox = FBox(positions);
			}
			out = out.isEmpty() ? bbox : sum(out, bbox);
		}

	return out;
}

void Model::join(const Model &other, const string &name) {}

void Model::draw(Renderer &out, const ModelPose &pose, PMaterial material,
				 const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	const auto &final_pose = finalPose(pose);
	for(const auto &node : nodes())
		if(node->meshId() != -1) {
			const auto &mesh = m_meshes[node->meshId()];
			const auto &skin = m_mesh_skins[node->meshId()];

			if(skin.empty()) {
				mesh.draw(out, material, final_pose[node->id()]);
			} else { Mesh(animateMesh(node->id(), pose)).draw(out, material, Matrix4::identity()); }
		}

	out.popViewMatrix();
}

void Model::drawNodes(Renderer &out, const ModelPose &pose, Color node_color, Color line_color,
					  const Matrix4 &matrix) const {
	Mesh bbox_mesh(MakeBBox(), FBox(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f));
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	const auto &final_pose = finalSkinnedPose(pose);
	vector<float3> positions(nodes().size());
	for(int n = 0; n < (int)nodes().size(); n++)
		positions[n] = mulPoint(final_pose[n], m_bind_matrices[n][3].xyz());

	auto material = make_shared<Material>(node_color);
	for(const auto *node : nodes()) {
		if(m_inv_bind_matrices[node->id()] != Matrix4::identity())
			bbox_mesh.draw(out, material, translation(positions[node->id()]));
		if(node->parentId() != -1) {
			vector<float3> lines{positions[node->id()], positions[node->parentId()]};
			out.addLines(lines, line_color);
		}
	}

	out.popViewMatrix();
}

void Model::printHierarchy() const {
	for(const auto *node : nodes())
		printf("%d: %s\n", node->id(), node->name().c_str());
}

Mesh Model::toMesh(const ModelPose &pose) const {
	vector<Mesh> meshes;

	const auto &final_pose = finalPose(pose);
	for(const auto *node : nodes())
		if(node->meshId() != -1) {
			const auto &mesh = m_meshes[node->meshId()];
			const auto &skin = m_mesh_skins[node->meshId()];

			meshes.emplace_back(skin.empty() ? Mesh::transform(final_pose[node->id()], mesh)
											 : animateMesh(node->id(), pose));
		}

	return Mesh::merge(meshes);
}

float Model::intersect(const Segment &segment, const ModelPose &pose) const {
	float min_isect = constant::inf;

	const auto &final_pose = finalPose(pose);
	for(const auto &node : nodes())
		if(node->meshId() != -1) {
			const auto &mesh = m_meshes[node->meshId()];
			const auto &skin = m_mesh_skins[node->meshId()];

			if(skin.empty()) {
				float isect = mesh.intersect(inverse(final_pose[node->id()]) * segment);
				min_isect = min(min_isect, isect);
			} else {
				PodArray<float3> positions(mesh.vertexCount());
				animateVertices(node->id(), pose, positions);

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
	DASSERT(node_id >= 0 && node_id < (int)nodes().size());

	const auto &node = nodes()[node_id];
	DASSERT(node->meshId() != -1);

	const MeshSkin &skin = m_mesh_skins[node->meshId()];
	const Mesh &mesh = m_meshes[node->meshId()];
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
	DASSERT(node_id >= 0 && node_id < (int)nodes().size());

	const auto &node = nodes()[node_id];
	DASSERT(node->meshId() != -1);
	const auto &mesh = m_meshes[node->meshId()];

	vector<float3> positions = mesh.positions();
	vector<float2> tex_coords = mesh.texCoords();
	animateVertices(node_id, pose, positions);

	return Mesh({positions, {}, tex_coords}, mesh.indices(), mesh.materials());
}

Matrix4 Model::nodeTrans(const string &name, const ModelPose &pose) const {
	const auto &final_pose = finalPose(pose);
	if(const auto *node = findNode(name))
		return final_pose[node->id()];
	return Matrix4::identity();
}

const vector<Matrix4> &Model::finalPose(const ModelPose &pose) const {
	DASSERT(pose.size() == nodes().size());
	if(pose.m_is_dirty) {
		auto &out = pose.m_final;
		out = std::vector<Matrix4>(begin(pose.m_transforms), end(pose.m_transforms));
		for(int n = 0; n < (int)out.size(); n++)
			if(nodes()[n]->parentId() != -1)
				out[n] = out[nodes()[n]->parentId()] * out[n];
		pose.m_is_dirty = false;
	}
	return pose.m_final;
}

const vector<Matrix4> &Model::finalSkinnedPose(const ModelPose &pose) const {
	DASSERT(pose.size() == nodes().size());
	if(pose.m_is_skinned_dirty) {
		const auto &normal_poses = Model::finalPose(pose);
		DASSERT(normal_poses.size() == nodes().size());

		auto &out = pose.m_skinned_final;
		out.resize(nodes().size());
		for(int n = 0; n < (int)out.size(); n++)
			out[n] = scaling(m_bind_scale) * normal_poses[n] * m_inv_bind_matrices[n];

		pose.m_is_skinned_dirty = false;
	}
	return pose.m_skinned_final;
}

ModelPose Model::defaultPose() const {
	vector<AffineTrans> out(nodes().size());
	for(int n = 0; n < (int)nodes().size(); n++)
		out[n] = nodes()[n]->localTrans();
	return ModelPose(std::move(out));
}

ModelPose Model::animatePose(int anim_id, double anim_pos) const {
	DASSERT(anim_id >= -1 && anim_id < (int)m_anims.size());

	if(anim_id == -1)
		return defaultPose();
	return m_anims[anim_id].animatePose(defaultPose(), anim_pos);
}
}
