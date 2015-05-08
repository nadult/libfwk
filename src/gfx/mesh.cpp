/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>
.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/scene.h>		// Output data structure
#include <assimp/postprocess.h> // Post processing flags

namespace fwk {

namespace {
	class StreamWrapper : public Assimp::IOStream {
	  public:
		StreamWrapper(Stream &stream) : m_stream(stream) {}
		size_t Read(void *buffer, size_t size, size_t count) {
			m_stream.loadData(buffer, size * count);
			return count;
		}

		size_t Write(const void *buffer, size_t size, size_t count) {
			m_stream.saveData(buffer, size * count);
			return count;
		}

		aiReturn Seek(size_t offset, aiOrigin origin) {
			if(origin == aiOrigin_SET)
				m_stream.seek(offset);
			else if(origin == aiOrigin_CUR)
				m_stream.seek(offset + m_stream.pos());
			else if(origin == aiOrigin_END) {
				DASSERT(offset < m_stream.size());
				m_stream.seek(m_stream.size() - offset);
			}
			return aiReturn_SUCCESS;
		}

		size_t Tell() const { return m_stream.pos(); }
		size_t FileSize() const { return m_stream.size(); }
		void Flush() {}

	  private:
		Stream &m_stream;
	};

	class SingleFileSystem : public Assimp::IOSystem {
	  public:
		SingleFileSystem(Stream &stream) : m_stream(stream) {}
		bool Exists(const char *name) const override { return strcmp(m_stream.name(), name) == 0; }
		char getOsSeparator() const override { return '/'; }
		Assimp::IOStream *Open(const char *name, const char *mode) override {
			DASSERT(mode && name);
			if(strchr(mode, 'w'))
				DASSERT(m_stream.isSaving());
			if(strchr(mode, 'r'))
				DASSERT(m_stream.isLoading());
			return new StreamWrapper(m_stream);
		}
		void Close(Assimp::IOStream *file) override { delete file; }

	  private:
		Stream &m_stream;
	};
}

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

SimpleMeshData::SimpleMeshData(MakeRect, const FRect &xy_rect, float z)
	: m_positions{float3(xy_rect.min.x, xy_rect.min.y, z), float3(xy_rect.max.x, xy_rect.min.y, z),
				  float3(xy_rect.max.x, xy_rect.max.y, z), float3(xy_rect.min.x, xy_rect.max.y, z)},
	  m_normals(4, float3(0, 0, 1)), m_tex_coords{{0, 0}, {1, 0}, {1, 1}, {0, 1}},
	  m_indices{0, 1, 2, 0, 2, 3}, m_primitive_type(PrimitiveType::triangles) {}

SimpleMeshData::SimpleMeshData(MakeBBox, const FBox &bbox) {
	/*	float3 corners[8];
		float2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

		bbox.getCorners(corners);

		int order[] = {1, 3, 2, 0, 1, 0, 4, 5, 5, 4, 6, 7, 3, 1, 5, 7, 2, 6, 4, 0, 3, 7, 6, 2};
		Mesh::Vertex verts[36];

		for(int s = 0; s < 6; s++) {
			float3 pos[4];
			float2 pos_uv[4];
			Color col[4];
			int tindex[6] = {2, 1, 0, 3, 2, 0};

			for(int i = 0; i < 6; i++) {
				int index = order[tindex[i] + s * 4];

				verts[s * 6 + i].pos = corners[index];
				verts[s * 6 + i].uv = uv[tindex[i]];
			}
		}

		return make_shared<Mesh>(verts, arraySize(verts), PrimitiveType::triangles);*/
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

/*
MeshData::MeshData(const collada::Root &croot) {
	for(int n = 0; n < croot.meshCount(); n++) {
		m_nodes.emplace_back(SimpleMeshData(croot.mesh(n)), Matrix4::identity());
	}
}

static collada::Root loadCollada(Stream &stream) {
	XMLDocument doc;
	stream >> doc;
	return collada::Root(doc);
}*/
MeshData::MeshData(const string &name, Stream &stream) {
	Assimp::Importer importer;
	importer.SetIOHandler(new SingleFileSystem(stream));

	int flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType;
	const aiScene *scene = importer.ReadFile(stream.name(), flags);
	if(!scene)
		THROW("Error while loading mesh: %s\n%s", name.c_str(), importer.GetErrorString());

	for(uint n = 0; n < scene->mNumMeshes; n++) {
		const auto *assmesh = scene->mMeshes[n];
		if(!assmesh->HasPositions() || !assmesh->HasFaces() || assmesh->mNumVertices > 65536)
			continue;

		vector<float3> positions;
		vector<float2> tex_coords;

		for(uint n = 0; n < assmesh->mNumVertices; n++) {
			const auto &pos = assmesh->mVertices[n];
			positions.emplace_back(pos.x, pos.y, pos.z);
		}

		if(assmesh->HasTextureCoords(0))
			for(uint n = 0; n < assmesh->mNumVertices; n++) {
				const auto &uv = assmesh->mTextureCoords[0][n];
				tex_coords.emplace_back(uv.x, -uv.y);
			}
		else
			tex_coords.resize(assmesh->mNumVertices, float2(0, 0));

		vector<u16> indices(assmesh->mNumFaces * 3);
		for(uint n = 0; n < assmesh->mNumFaces; n++) {
			ASSERT(assmesh->mFaces[n].mNumIndices == 3);
			for(int i = 0; i < 3; i++)
				indices[n * 3 + i] = assmesh->mFaces[n].mIndices[i];
		}
		m_nodes.emplace_back(SimpleMeshData(positions, tex_coords, indices), Matrix4::identity());
	}
}

Mesh::Mesh(const string &name, Stream &stream) : Mesh(MeshData(name, stream)) {}

Mesh::Mesh(const MeshData &data) {
	for(const auto &node : data.m_nodes)
		m_nodes.emplace_back(SimpleMesh(node.mesh), node.matrix);
}

void Mesh::draw(Renderer &out, const Material &material, const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	for(const auto &node : m_nodes) {
		out.pushViewMatrix();
		out.mulViewMatrix(node.matrix);
		node.mesh.draw(out, material);
		out.popViewMatrix();
	}
	out.popViewMatrix();
}

/*

static bool getRootMatrix(XMLNode node, const Matrix4 &parent_mat, Matrix4 &out,
						  StringRef mesh_id) {
	XMLNode mat_node = node.child("matrix");
	Matrix4 local = parent_mat;

		if(mat_node) {
			mat_node.parseValues(&local[0][0], 16);
			local = parent_mat * transpose(local);
		}

		XMLNode inst_node = node.child("instance_geometry");
		if(inst_node) {
			StringRef name = inst_node ? inst_node.weakAttrib("url") : "";
			if(name[0] == '#' && name + 1 == mesh_id) {
				out = local;
				return true;
			}
		}

		XMLNode child = node.child("node");
		while(child) {
			if(getRootMatrix(child, local, out, mesh_id))
				return true;
			child.next();
		}

	return false;
}

static Matrix4 getRootMatrix(const collada::Root &root, StringRef mesh_id) {
	Matrix4 out = identity();

	for(int n = 0; n < root.sceneNodeCount(); n++) {
		XMLNode node = root.sceneNode(n).node();
		if(getRootMatrix(node, identity(), out, mesh_id))
			return out;
		node.next();
	}

	return out;
}
*/

/*
float Mesh::trace(const Segment &segment) const {
	float dist = constant::inf;
	int tri_count = vertexCount() / 3;
	for(int n = 0; n < tri_count; n++)
		dist = min(dist, intersection(segment, m_positions[n * 3 + 0], m_positions[n * 3 + 1],
									  m_positions[n * 3 + 2]));

	return dist;
}*/

// const Stream Mesh::toStream() const {
//	return Stream(m_primitive_type, m_positions.size(), nullptr, m_positions.data(),
//				  nullptr /*m_normals.data()*/, m_tex_coords.data());
//}
}
