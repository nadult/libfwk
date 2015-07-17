/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <deque>
#include <unordered_map>
#include "fwk_profile.h"

namespace fwk {

Model::MeshSkin::MeshSkin(const XMLNode &node) {
	using namespace xml_conversions;
	XMLNode counts_node = node.child("counts");
	XMLNode weights_node = node.child("weights");
	XMLNode node_ids_node = node.child("node_ids");
	if(!counts_node && !weights_node && !node_ids_node)
		return;

	ASSERT(counts_node && weights_node && node_ids_node);
	auto counts = fromString<vector<int>>(counts_node.value());
	auto weights = fromString<vector<float>>(weights_node.value());
	auto node_ids = fromString<vector<int>>(node_ids_node.value());

	ASSERT(weights.size() == node_ids.size());
	ASSERT(std::accumulate(begin(counts), end(counts), 0) == (int)weights.size());

	vertex_weights.resize(counts.size());
	int offset = 0;
	for(int n = 0; n < (int)counts.size(); n++) {
		auto &out = vertex_weights[n];
		out.resize(counts[n]);
		for(int i = 0; i < counts[n]; i++)
			out[i] = VertexWeight(weights[offset + i], node_ids[offset + i]);
		offset += counts[n];
	}
}

void Model::MeshSkin::saveToXML(XMLNode node) const {
	if(isEmpty())
		return;

	vector<int> counts;
	vector<float> weights;
	vector<int> node_ids;

	for(int n = 0; n < (int)vertex_weights.size(); n++) {
		const auto &in = vertex_weights[n];
		counts.emplace_back((int)in.size());
		for(int i = 0; i < (int)in.size(); i++) {
			weights.emplace_back(in[i].weight);
			node_ids.emplace_back(in[i].node_id);
		}
	}

	using namespace xml_conversions;
	node.addChild("counts", node.own(toString(counts)));
	node.addChild("weights", node.own(toString(weights)));
	node.addChild("node_ids", node.own(toString(node_ids)));
}

bool Model::MeshSkin::isEmpty() const {
	return std::all_of(begin(vertex_weights), end(vertex_weights),
					   [](const auto &v) { return v.empty(); });
}

Model::Model(const aiScene &ascene) {
	auto *root_node = ascene.mRootNode;
	Matrix4 mat(root_node->mTransformation[0]);

	m_meshes.reserve(ascene.mNumMeshes);
	for(uint n = 0; n < ascene.mNumMeshes; n++)
		m_meshes.emplace_back(SimpleMesh(ascene, n));

	std::deque<pair<const aiNode *, int>> queue({make_pair(root_node, -1)});

	while(!queue.empty()) {
		auto *anode = queue.front().first;
		int parent_id = queue.front().second;
		queue.pop_front();

		int node_id =
			addNode(anode->mName.C_Str(),
					AffineTrans(transpose(Matrix4(anode->mTransformation[0]))), parent_id);
		Node &new_node = m_nodes[node_id];
		if(anode->mNumMeshes > 1) {
			for(uint n = 0; n < anode->mNumMeshes; n++) {
				int mesh_node_id = addNode("", AffineTrans(), node_id);
				auto &mesh_node = m_nodes[mesh_node_id];
				mesh_node.mesh_id = anode->mMeshes[n];
			}
		} else if(anode->mNumMeshes == 1) { new_node.mesh_id = anode->mMeshes[0]; }

		for(uint c = 0; c < anode->mNumChildren; c++)
			queue.emplace_back(anode->mChildren[c], node_id);
	}

	for(uint a = 0; a < ascene.mNumAnimations; a++)
		m_anims.emplace_back(ascene, (int)a, *this);

	m_mesh_skins.resize(m_meshes.size());
	for(uint m = 0; m < ascene.mNumMeshes; m++) {
		const aiMesh &amesh = *ascene.mMeshes[m];
		ASSERT((int)amesh.mNumVertices == m_meshes[m].vertexCount());
		MeshSkin &skin = m_mesh_skins[m];
		skin.vertex_weights.resize(amesh.mNumVertices);

		for(uint n = 0; n < amesh.mNumBones; n++) {
			const aiBone &abone = *amesh.mBones[n];
			int node_id = findNode(abone.mName.C_Str());
			ASSERT(node_id != -1);
			const auto &joint = m_nodes[node_id];

			for(uint w = 0; w < abone.mNumWeights; w++) {
				const aiVertexWeight &aweight = abone.mWeights[w];
				skin.vertex_weights[aweight.mVertexId].emplace_back(aweight.mWeight, node_id);
			}
		}
	}

	computeBindMatrices();
	verifyData();
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

	XMLNode mesh_node = node.child("simple_mesh");
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
		m_materials[mat_node.attrib("name")] =
			make_shared<Material>(Color(mat_node.attrib<float3>("diffuse")));
		mat_node.next();
	}

	computeBindMatrices();
	for(auto &mesh : m_meshes)
		mesh.assignMaterials(m_materials);
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

		ASSERT((int)skin.vertex_weights.size() == mesh.vertexCount() || skin.isEmpty());
		for(const auto &vweights : skin.vertex_weights) {
			for(const auto &weight : vweights) {
				ASSERT(weight.node_id >= 0 && weight.node_id < (int)m_nodes.size());
				ASSERT(weight.weight >= 0 && weight.weight <= 1.0f);
			}
		}
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

		XMLNode mesh_node = xml_node.addChild("simple_mesh");
		mesh.saveToXML(mesh_node);
		if(!skin.isEmpty())
			skin.saveToXML(mesh_node.addChild("skin"));
	}
	for(const auto &mat : m_materials) {
		XMLNode mat_node = xml_node.addChild("material");
		mat_node.addAttrib("name", mat_node.own(mat.first));
		mat_node.addAttrib("diffuse", float3(mat.second->color()));
	}
	for(const auto &anim : m_anims)
		anim.saveToXML(xml_node.addChild("anim"));
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
			if(skin.isEmpty()) {
				bbox = final_pose[node.id] * mesh.boundingBox();
			} else {
				PodArray<float3> positions(mesh.vertexCount());
				animateVertices(node.id, pose, positions.data(), nullptr);
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

			if(skin.isEmpty()) {
				mesh.draw(out, material, final_pose[node.id]);
			} else {
				SimpleMesh(animateMesh(node.id, pose)).draw(out, material, Matrix4::identity());
			}
		}

	out.popViewMatrix();
}

void Model::drawNodes(Renderer &out, const ModelPose &pose, Color node_color, Color line_color,
					  const Matrix4 &matrix) const {
	SimpleMesh bbox_mesh(MakeBBox(), FBox(-0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f));
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

SimpleMesh Model::toSimpleMesh(const ModelPose &pose) const {
	vector<SimpleMesh> meshes;

	const auto &final_pose = finalPose(pose);
	for(const auto &node : m_nodes)
		if(node.mesh_id != -1) {
			const auto &mesh = m_meshes[node.mesh_id];
			const auto &skin = m_mesh_skins[node.mesh_id];

			meshes.emplace_back(skin.isEmpty() ? SimpleMesh::transform(final_pose[node.id], mesh)
											   : animateMesh(node.id, pose));
		}

	return SimpleMesh::merge(meshes);
}

float Model::intersect(const Segment &segment, const ModelPose &pose) const {
	float min_isect = constant::inf;

	const auto &final_pose = finalPose(pose);
	for(const auto &node : m_nodes)
		if(node.mesh_id != -1) {
			const auto &mesh = m_meshes[node.mesh_id];
			const auto &skin = m_mesh_skins[node.mesh_id];

			if(skin.isEmpty()) {
				float isect = mesh.intersect(inverse(final_pose[node.id]) * segment);
				min_isect = min(min_isect, isect);
			} else {
				PodArray<float3> positions(mesh.vertexCount());
				animateVertices(node.id, pose, positions.data(), nullptr);

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

void Model::animateVertices(int node_id, const ModelPose &pose, float3 *out_positions,
							float3 *out_normals) const {
	DASSERT(node_id >= 0 && node_id < (int)m_nodes.size());

	const auto &node = m_nodes[node_id];
	DASSERT(node.mesh_id != -1);

	const MeshSkin &skin = m_mesh_skins[node.mesh_id];
	const SimpleMesh &mesh = m_meshes[node.mesh_id];
	updateCounter("Model::animateVertices", 1);
	FWK_PROFILE("Model::animateVertices");
	DASSERT(!skin.isEmpty());

	vector<Matrix4> matrices = finalSkinnedPose(pose);
	Matrix4 offset_matrix = computeOffsetMatrix(node_id);

	for(auto &matrix : matrices)
		matrix = matrix * offset_matrix;

	for(int v = 0; v < (int)skin.vertex_weights.size(); v++) {
		const auto &vweights = skin.vertex_weights[v];

		if(out_positions) {
			float3 pos = mesh.positions()[v];
			float3 out;
			for(const auto &weight : vweights)
				out += mulPointAffine(matrices[weight.node_id], pos) * weight.weight;
			out_positions[v] = out;
		}
		if(out_normals) {
			float3 nrm = mesh.normals()[v];
			float3 out;
			for(const auto &weight : vweights)
				out += mulNormalAffine(matrices[weight.node_id], nrm) * weight.weight;
			out_normals[v] = out;
		}
	}
}

SimpleMesh Model::animateMesh(int node_id, const ModelPose &pose) const {
	DASSERT(node_id >= 0 && node_id < (int)m_nodes.size());

	const auto &node = m_nodes[node_id];
	DASSERT(node.mesh_id != -1);
	const auto &mesh = m_meshes[node.mesh_id];

	vector<float3> positions = mesh.positions();
	vector<float2> tex_coords = mesh.texCoords();
	animateVertices(node_id, pose, positions.data(), nullptr);

	return SimpleMesh({positions, {}, tex_coords}, mesh.indices(), mesh.materials());
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
