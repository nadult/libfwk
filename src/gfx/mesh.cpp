/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <deque>

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

SimpleMeshData::SimpleMeshData(MakeRect, const FRect &xz_rect, float y)
	: m_positions{float3(xz_rect.min[0], y, xz_rect.min[1]),
				  float3(xz_rect.max[0], y, xz_rect.min[1]),
				  float3(xz_rect.max[0], y, xz_rect.max[1]),
				  float3(xz_rect.min[0], y, xz_rect.max[1])},
	  m_normals(4, float3(0, 1, 0)), m_tex_coords{{0, 0}, {1, 0}, {1, 1}, {0, 1}},
	  m_indices{0, 2, 1, 0, 3, 2}, m_primitive_type(PrimitiveType::triangles) {}

SimpleMeshData::SimpleMeshData(MakeBBox, const FBox &bbox) {
	float3 corners[8];
	float2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

	bbox.getCorners(corners);

	int order[] = {1, 3, 2, 0, 1, 0, 4, 5, 5, 4, 6, 7, 3, 1, 5, 7, 2, 6, 4, 0, 3, 7, 6, 2};

	m_positions.reserve(24);
	m_tex_coords.reserve(24);
	m_indices.reserve(36);

	for(int s = 0; s < 6; s++) {
		for(int i = 0; i < 4; i++) {
			m_positions.emplace_back(corners[order[s * 4 + i]]);
			m_tex_coords.emplace_back(uvs[i]);
		}

		int face_indices[6] = {2, 1, 0, 3, 2, 0};
		for(int i = 0; i < 6; i++)
			m_indices.push_back(s * 4 + face_indices[i]);
	}
	m_primitive_type = PrimitiveType::triangles;
}

SimpleMeshData::SimpleMeshData(const aiScene &ascene, int mesh_id) {
	DASSERT(mesh_id >= 0 && mesh_id < ascene.mNumMeshes);
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
}

SimpleMeshData::SimpleMeshData(const vector<float3> &positions, const vector<float2> &tex_coords,
							   const vector<u16> &indices)
	: m_positions(positions), m_tex_coords(tex_coords), m_indices(indices),
	  m_primitive_type(PrimitiveType::triangles) {}

void SimpleMeshData::transformUV(const Matrix4 &matrix) {
	for(int n = 0; n < (int)m_tex_coords.size(); n++)
		m_tex_coords[n] = (matrix * float4(m_tex_coords[n], 0.0f, 1.0f)).xy();
}

SimpleMesh::SimpleMesh(const SimpleMeshData &data, Color color)
	: m_vertex_array(VertexArray::make({make_shared<VertexBuffer>(data.m_positions), color,
										make_shared<VertexBuffer>(data.m_tex_coords)},
									   make_shared<IndexBuffer>(data.m_indices))),
	  m_primitive_type(data.m_primitive_type) {}

void SimpleMesh::draw(Renderer &renderer, const Material &material, const Matrix4 &mat) const {
	renderer.addDrawCall({m_vertex_array, m_primitive_type, m_vertex_array->size(), 0}, material,
						 mat);
}

MeshData::MeshData(const aiScene &ascene) {
	Matrix4 mat(ascene.mRootNode->mTransformation[0]);

	m_meshes.reserve(ascene.mNumMeshes);
	for(uint n = 0; n < ascene.mNumMeshes; n++)
		m_meshes.emplace_back(SimpleMeshData(ascene, n));

	std::deque<pair<const aiNode *, int>> queue({make_pair(ascene.mRootNode, -1)});

	while(!queue.empty()) {
		auto *anode = queue.front().first;
		int parent_id = queue.front().second;
		queue.pop_front();

		Node new_node;
		new_node.name = anode->mName.C_Str();
		//TODO: why transpose is needed?
		new_node.trans = transpose(Matrix4(anode->mTransformation[0]));
		new_node.parent_id = parent_id;
		
		for(uint m = 0; m < anode->mNumMeshes; m++)
			new_node.mesh_ids.emplace_back(anode->mMeshes[m]);

		parent_id = (int)m_nodes.size();
		m_nodes.emplace_back(new_node);


		for(uint c = 0; c < anode->mNumChildren; c++)
			queue.emplace_back(anode->mChildren[c], parent_id);
	}
}

Mesh::Mesh(const aiScene &scene) : Mesh(MeshData(scene)) {}

Mesh::Mesh(const MeshData &data) :m_nodes(data.nodes()) {
	m_meshes.reserve(data.meshes().size());
	for(const auto &mesh : data.meshes())
		m_meshes.emplace_back(mesh);
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

/*
float Mesh::trace(const Segment &segment) const {
	float dist = constant::inf;
	int tri_count = vertexCount() / 3;
	for(int n = 0; n < tri_count; n++)
		dist = min(dist, intersection(segment, m_positions[n * 3 + 0], m_positions[n * 3 + 1],
									  m_positions[n * 3 + 2]));

	return dist;
}*/

}
