/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <deque>
#include <unordered_map>

namespace fwk {

/*
void Mesh::genAdjacency() {
	struct TVec : public float3 {
		TVec(const float3 &vec) : float3(vec) {}
		TVec() {}

		bool operator<(const TVec &rhs) const {
			return v[0] == rhs[0] ? v[1] == rhs[1] ? v[2] < rhs[2] : v[1] < rhs[1] : v[0] < rhs[0];
		}
	};

	struct Edge {
		Edge(const TVec &a, const TVec &b, int faceIdx) : faceIdx(faceIdx) {
			v[0] = a;
			v[1] = b;
			if(v[0] < v[1])
				swap(v[0], v[1]);
		}

		bool operator<(const Edge &rhs) const {
			return v[0] == rhs.v[0] ? v[1] < rhs.v[1] : v[0] < rhs.v[0];
		}

		bool operator==(const Edge &rhs) const { return v[0] == rhs.v[0] && v[1] == rhs.v[1]; }

		TVec v[2];
		int faceIdx;
	};

	vector<Edge> edges;
	for(int f = 0; f < faceCount(); f++) {
		int idx[3];
		getFace(f, idx[0], idx[1], idx[2]);
		edges.push_back(Edge(m_positions[idx[0]], m_positions[idx[1]], f));
		edges.push_back(Edge(m_positions[idx[1]], m_positions[idx[2]], f));
		edges.push_back(Edge(m_positions[idx[2]], m_positions[idx[0]], f));
	}
	std::sort(edges.begin(), edges.end());

	m_neighbours.resize(faceCount() * 3);
	for(int f = 0; f < faceCount(); f++) {
		int idx[3];
		getFace(f, idx[0], idx[1], idx[2]);

		for(int i = 0; i < 3; i++) {
			Edge edge(m_positions[idx[i]], m_positions[idx[(i + 1) % 3]], ~0u);
			auto edg = lower_bound(edges.begin(), edges.end(), edge);

			int neighbour = ~0u;
			while(edg != edges.end() && *edg == edge) {
				if(edg->faceIdx != f) {
					neighbour = edg->faceIdx;
					break;
				}
				++edg;
			}
			m_neighbours[f * 3 + i] = neighbour;
		}
	}
}

int Mesh::faceCount() const {
	return m_primitive_type == PrimitiveType::triangle_strip ? m_positions.size() - 2
															 : m_positions.size() / 3;
}

void Mesh::getFace(int face, int &i1, int &i2, int &i3) const {
	if(m_primitive_type == PrimitiveType::triangle_strip) {
		i1 = face + 0;
		i2 = face + 1;
		i3 = face + 2;
	} else {
		i1 = face * 3 + 0;
		i2 = face * 3 + 1;
		i3 = face * 3 + 2;
	}
}*/

SimpleMesh::SimpleMesh(const aiScene &ascene, int mesh_id) {
	DASSERT(mesh_id >= 0 && mesh_id < (int)ascene.mNumMeshes);
	const auto *amesh = ascene.mMeshes[mesh_id];

	DASSERT(amesh->HasPositions() && amesh->HasFaces() && amesh->mNumVertices <= 65536);

	for(uint n = 0; n < amesh->mNumVertices; n++) {
		const auto &pos = amesh->mVertices[n];
		m_positions.emplace_back(pos.x, pos.y, pos.z);
	}

	if(amesh->HasTextureCoords(0))
		for(uint n = 0; n < amesh->mNumVertices; n++) {
			const auto &uv = amesh->mTextureCoords[0][n];
			m_tex_coords.emplace_back(uv.x, -uv.y);
		}
	else {
		// TODO: fixme
		m_tex_coords.resize(amesh->mNumVertices, float2(0, 0));
	}

	m_indices.resize(amesh->mNumFaces * 3);
	for(uint n = 0; n < amesh->mNumFaces; n++) {
		ASSERT(amesh->mFaces[n].mNumIndices == 3);
		for(int i = 0; i < 3; i++)
			m_indices[n * 3 + i] = amesh->mFaces[n].mIndices[i];
	}
	m_primitive_type = PrimitiveType::triangles;
	computeBoundingBox();
}

SimpleMesh::SimpleMesh()
	: m_primitive_type(PrimitiveType::triangles), m_is_drawing_cache_dirty(true) {}

SimpleMesh::SimpleMesh(vector<float3> positions, vector<float3> normals, vector<float2> tex_coords,
					   vector<uint> indices, PrimitiveType::Type prim_type)
	: m_positions(std::move(positions)), m_normals(std::move(normals)),
	  m_tex_coords(std::move(tex_coords)), m_indices(std::move(indices)),
	  m_primitive_type(prim_type), m_is_drawing_cache_dirty(true) {
	DASSERT(m_tex_coords.size() == m_positions.size() || m_tex_coords.empty());
	DASSERT(m_normals.size() == m_positions.size() || m_normals.empty());
	for(auto idx : indices)
		DASSERT(idx < m_positions.size());
	computeBoundingBox();
}
SimpleMesh::SimpleMesh(PVertexBuffer positions, PVertexBuffer normals, PVertexBuffer tex_coords,
					   PIndexBuffer indices, PrimitiveType::Type prim_type)
	: SimpleMesh((DASSERT(positions), positions->getData<float3>()),
				 normals ? normals->getData<float3>() : vector<float3>(),
				 tex_coords ? tex_coords->getData<float2>() : vector<float2>(),
				 indices ? indices->getData() : vector<uint>(), prim_type) {}

template <class T> static PVertexBuffer extractBuffer(PVertexArray array, int buffer_id) {
	DASSERT(array);
	const auto &sources = array->sources();
	DASSERT(buffer_id == -1 || (buffer_id >= 0 && buffer_id < sources.size()));
	return buffer_id == -1 ? PVertexBuffer() : sources[buffer_id].buffer();
}

SimpleMesh::SimpleMesh(PVertexArray array, int positions_id, int normals_id, int tex_coords_id,
					   PrimitiveType::Type prim_type)
	: SimpleMesh(extractBuffer<float3>(array, positions_id),
				 extractBuffer<float3>(array, normals_id),
				 extractBuffer<float2>(array, tex_coords_id),
				 (DASSERT(array), array->indexBuffer()), prim_type) {}

void SimpleMesh::transformUV(const Matrix4 &matrix) {
	for(int n = 0; n < (int)m_tex_coords.size(); n++)
		m_tex_coords[n] = (matrix * float4(m_tex_coords[n], 0.0f, 1.0f)).xy();
	m_is_drawing_cache_dirty = true;
}

void SimpleMesh::computeBoundingBox() { m_bounding_box = FBox(m_positions); }

vector<SimpleMesh::TriIndices> SimpleMesh::trisIndices() const {
	vector<TriIndices> out;
	// TODO: remove degenerate triangles

	if(m_indices.size() < 3)
		return out;

	if(m_primitive_type == PrimitiveType::triangles) {
		out.reserve(m_indices.size() / 3);
		for(int n = 0; n < (int)m_indices.size(); n += 3)
			out.push_back({{m_indices[n + 0], m_indices[n + 1], m_indices[n + 2]}});
	} else if(m_primitive_type == PrimitiveType::triangle_strip) {
		for(int n = 2; n < (int)m_indices.size(); n++) {
			uint a = m_indices[n - 2];
			uint b = m_indices[n - 1];
			uint c = m_indices[n];

			if(a != b && b != c && a != c)
				out.push_back({{a, b, c}});
		}
	} else
		THROW("Add support for different primitive types");

	return out;
}

// TODO: test split / merge and transform
vector<SimpleMesh> SimpleMesh::split(int max_vertices) const {
	DASSERT(max_vertices >= 3 && !m_indices.empty());
	vector<SimpleMesh> out;

	int last_index = 0;

	while(true) {
		out.push_back(SimpleMesh());
		auto &new_mesh = out.back();

		int num_vertices = 0;
		std::unordered_map<uint, uint> index_map;
		vector<pair<uint, uint>> index_list;

		if(m_indices.empty()) {
			for(int n = 0; n < num_vertices; n++)
				index_list.emplace_back(n, n);
		} else
			for(int i = last_index; i < (int)m_indices.size(); i++) {
				uint vindex = m_indices[i];
				auto it = index_map.find(vindex);
				if(it == index_map.end()) {
					it = index_map.emplace(vindex, num_vertices++).first;
				}
				index_list.emplace_back(vindex, it->second);

				if(num_vertices >= max_vertices) {
					last_index = i + 1;
					break;
				}
			}

		new_mesh.m_positions.resize(num_vertices);
		for(auto index_pair : index_list)
			new_mesh.m_positions[index_pair.second] = m_positions[index_pair.first];

		if(!m_normals.empty()) {
			new_mesh.m_normals.resize(num_vertices);
			for(auto index_pair : index_list)
				new_mesh.m_normals[index_pair.second] = m_normals[index_pair.first];
		}
		if(!m_tex_coords.empty()) {
			new_mesh.m_tex_coords.resize(num_vertices);
			for(auto index_pair : index_list)
				new_mesh.m_tex_coords[index_pair.second] = m_tex_coords[index_pair.first];
		}

		if(!m_indices.empty()) {
			new_mesh.m_indices.resize(index_list.size());
			for(int n = 0; n < (int)index_list.size(); n++)
				new_mesh.m_indices[n] = index_list[n].second;
		}
		new_mesh.m_primitive_type = m_primitive_type;
		new_mesh.computeBoundingBox();
	}

	return out;
}

SimpleMesh SimpleMesh::merge(const vector<SimpleMesh> &meshes) {
	SimpleMesh out;

	int num_vertices = 0, num_indices = 0;
	bool has_tex_coords = meshes.empty() ? false : meshes.front().hasTexCoords();
	bool has_normals = meshes.empty() ? false : meshes.front().hasNormals();
	bool need_indices = false;
	if(!meshes.empty())
		out.m_primitive_type = meshes.front().primitiveType();

	for(const auto &mesh : meshes) {
		num_vertices += (int)mesh.m_positions.size();
		need_indices |= !mesh.m_indices.empty();
		num_indices += mesh.m_indices.empty() ? num_vertices : (int)mesh.m_indices.size();
		DASSERT(has_tex_coords == mesh.hasTexCoords());
		DASSERT(has_normals == mesh.hasNormals());
		DASSERT(mesh.primitiveType() == out.primitiveType());
	}

	out.m_positions.resize(num_vertices);
	out.m_indices.resize(need_indices ? num_indices : 0);
	out.m_tex_coords.resize(has_tex_coords ? num_vertices : 0);
	out.m_normals.resize(has_normals ? num_vertices : 0);

	int last_vertex = 0;
	int last_index = 0;

	for(const auto &mesh : meshes) {
		std::copy(begin(mesh.m_positions), end(mesh.m_positions),
				  begin(out.m_positions) + last_vertex);
		if(has_tex_coords)
			std::copy(begin(mesh.m_tex_coords), end(mesh.m_tex_coords),
					  begin(out.m_tex_coords) + last_vertex);
		if(has_normals)
			std::copy(begin(mesh.m_normals), end(mesh.m_normals),
					  begin(out.m_normals) + last_vertex);

		if(need_indices) {
			if(mesh.m_indices.empty()) {
				for(int n = 0; n < (int)mesh.m_positions.size(); n++)
					out.m_indices[last_index + n] = last_vertex + n;
			} else {
				std::copy(begin(mesh.m_indices), end(mesh.m_indices),
						  begin(out.m_indices) + last_index);
			}
		}
		last_vertex += (int)mesh.m_positions.size();
		last_index +=
			mesh.m_indices.empty() ? (int)mesh.m_positions.size() : (int)mesh.m_indices.size();
	}

	out.computeBoundingBox();
	return out;
}

vector<float3> transformVertices(const Matrix4 &mat, vector<float3> verts) {
	for(auto &vert : verts)
		vert = mulPoint(mat, vert);
	return verts;
}

vector<float3> transformNormals(const Matrix4 &mat, vector<float3> normals) {
	Matrix4 nrm_mat = transpose(inverse(mat));
	for(auto &nrm : normals)
		nrm = mulNormal(nrm_mat, nrm);
	return normals;
}

SimpleMesh SimpleMesh::transform(const Matrix4 &mat, SimpleMesh mesh) {
	mesh.m_positions = transformVertices(mat, std::move(mesh.m_positions));
	if(mesh.hasNormals())
		mesh.m_normals = transformNormals(mat, std::move(mesh.m_normals));
	return mesh;
}

static PVertexArray makeVertexArray(const SimpleMesh &data) {
	auto vertices = make_shared<VertexBuffer>(data.positions());
	auto tex_coords = data.hasTexCoords() ? make_shared<VertexBuffer>(data.texCoords())
										  : VertexArraySource(float2(0, 0));
	auto indices = data.hasIndices() ? make_shared<IndexBuffer>(data.indices()) : PIndexBuffer();
	return VertexArray::make({vertices, Color::white, tex_coords}, std::move(indices));
}

void SimpleMesh::draw(Renderer &out, const Material &material, const Matrix4 &matrix) const {
	if(m_is_drawing_cache_dirty) {
		m_is_drawing_cache_dirty = false;
		m_drawing_cache.clear();

		if(hasIndices() && m_positions.size() > IndexBuffer::max_index_value) {
			auto parts = split(IndexBuffer::max_index_value);
			for(const auto &part : parts)
				m_drawing_cache.emplace_back(makeVertexArray(part));
		} else { m_drawing_cache.emplace_back(makeVertexArray(*this)); }
	}

	for(const auto &varray : m_drawing_cache) {
		DrawCall draw_call(varray, m_primitive_type, varray->size(), 0);
		out.addDrawCall(draw_call, material, matrix);
	}
}

float SimpleMesh::intersect(const Segment &segment) const {
	float min_isect = constant::inf;

	if(intersection(segment, m_bounding_box) < constant::inf)
		for(const auto &indices : trisIndices()) {
			Triangle triangle(m_positions[indices[0]], m_positions[indices[1]],
							  m_positions[indices[2]]);
			min_isect = min(min_isect, intersection(segment, triangle));
		}

	return min_isect;
}

void SimpleMesh::clearDrawingCache() const {
	m_drawing_cache.clear();
	m_is_drawing_cache_dirty = true;
}

Mesh::Mesh(const aiScene &ascene) {
	Matrix4 mat(ascene.mRootNode->mTransformation[0]);

	m_meshes.reserve(ascene.mNumMeshes);
	for(uint n = 0; n < ascene.mNumMeshes; n++)
		m_meshes.emplace_back(SimpleMesh(ascene, n));

	std::deque<pair<const aiNode *, int>> queue({make_pair(ascene.mRootNode, -1)});

	while(!queue.empty()) {
		auto *anode = queue.front().first;
		int parent_id = queue.front().second;
		queue.pop_front();

		Node new_node;
		new_node.name = anode->mName.C_Str();
		// TODO: why transpose is needed?
		new_node.trans = transpose(Matrix4(anode->mTransformation[0]));
		new_node.parent_id = parent_id;

		for(uint m = 0; m < anode->mNumMeshes; m++)
			new_node.mesh_ids.emplace_back(anode->mMeshes[m]);

		parent_id = (int)m_nodes.size();
		m_nodes.emplace_back(new_node);

		for(uint c = 0; c < anode->mNumChildren; c++)
			queue.emplace_back(anode->mChildren[c], parent_id);
	}
	computeBoundingBox();
}

void Mesh::computeBoundingBox() {
	m_bounding_box = m_meshes.empty() ? FBox::empty() : m_meshes.front().boundingBox();
	for(const auto &mesh : m_meshes)
		m_bounding_box = sum(m_bounding_box, mesh.boundingBox());
}

int Mesh::findNode(const string &name) const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		if(m_nodes[n].name == name)
			return n;
	return -1;
}

void Mesh::draw(Renderer &out, const Material &material, const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	vector<Matrix4> matrices(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		matrices[n] = m_nodes[n].trans;
		if(m_nodes[n].parent_id != -1)
			matrices[n] = matrices[m_nodes[n].parent_id] * matrices[n];
		for(int mesh_id : m_nodes[n].mesh_ids)
			m_meshes[mesh_id].draw(out, material, matrices[n]);
	}

	out.popViewMatrix();
}

void Mesh::printHierarchy() const {
	for(int n = 0; n < (int)m_nodes.size(); n++)
		printf("%d: %s\n", n, m_nodes[n].name.c_str());
}

SimpleMesh Mesh::toSimpleMesh() const {
	vector<Matrix4> matrices(m_nodes.size());
	vector<SimpleMesh> meshes;

	for(int n = 0; n < (int)m_nodes.size(); n++) {
		matrices[n] = m_nodes[n].trans;
		if(m_nodes[n].parent_id != -1)
			matrices[n] = matrices[m_nodes[n].parent_id] * matrices[n];

		for(int mesh_id : m_nodes[n].mesh_ids)
			meshes.emplace_back(SimpleMesh::transform(matrices[n], m_meshes[mesh_id]));
	}

	return SimpleMesh::merge(meshes);
}

float Mesh::intersect(const Segment &segment) const {
	float min_isect = constant::inf;

	vector<Matrix4> matrices(m_nodes.size());
	for(int n = 0; n < (int)m_nodes.size(); n++) {
		matrices[n] = m_nodes[n].trans;
		if(m_nodes[n].parent_id != -1)
			matrices[n] = matrices[m_nodes[n].parent_id] * matrices[n];

		for(int mesh_id : m_nodes[n].mesh_ids) {
			float isect = m_meshes[mesh_id].intersect(inverse(matrices[n]) * segment);
			min_isect = min(min_isect, isect);
		}
	}

	return min_isect;
}
}
