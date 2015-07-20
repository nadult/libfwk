/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <deque>
#include <unordered_map>

namespace fwk {

MeshIndices::MeshIndices(vector<uint> indices, Type type)
	: m_data(std::move(indices)), m_type(type) {
	DASSERT(isSupported(m_type));
}

MeshIndices::MeshIndices(Range<uint> indices, Type type)
	: MeshIndices(vector<uint>(begin(indices), end(indices)), type) {}
MeshIndices::MeshIndices(PIndexBuffer indices, Type type) : MeshIndices(indices->getData(), type) {}

MeshIndices MeshIndices::merge(const vector<MeshIndices> &set,
							   vector<pair<uint, uint>> &index_ranges) {
	auto type = set.empty() ? Type::triangles : set.front().type();

	vector<uint> merged;
	index_ranges.clear();

	for(const auto &indices : set) {
		DASSERT(indices.type() == type);

		if(type == Type::triangle_strip && !merged.empty() && !indices.empty()) {
			uint prev = merged.back(), next = indices.m_data.front();

			if(merged.size() % 2 == 1) {
				uint indices[5] = {prev, prev, prev, next, next};
				merged.insert(end(merged), begin(indices), end(indices));
			} else {
				uint indices[4] = {prev, prev, next, next};
				merged.insert(end(merged), begin(indices), end(indices));
			}
		}

		index_ranges.emplace_back(merged.size(), indices.size());
		merged.insert(end(merged), begin(indices.m_data), end(indices.m_data));
	}

	return MeshIndices(merged, type);
}

int MeshIndices::triangleCount() const {
	int index_count = (int)m_data.size();
	return m_type == Type::triangles ? index_count / 3 : max(0, index_count - 2);
}

uint MeshIndices::maxIndex() const {
	uint out = 0;
	for(const auto idx : m_data)
		out = max(out, idx);
	return out;
}

vector<MeshIndices::TriIndices> MeshIndices::trisIndices() const {
	vector<TriIndices> out;
	out.reserve(triangleCount());

	if(m_type == Type::triangles) {
		for(uint n = 0; n + 2 < m_data.size(); n += 3)
			out.push_back({{m_data[n + 0], m_data[n + 1], m_data[n + 2]}});

	} else {
		DASSERT(m_type == Type::triangle_strip);
		bool do_swap = false;

		for(uint n = 2; n < m_data.size(); n++) {
			uint a = m_data[n - 2];
			uint b = m_data[n - 1];
			uint c = m_data[n];
			if(do_swap)
				swap(b, c);
			do_swap ^= 1;

			if(a != b && b != c && a != c)
				out.push_back({{a, b, c}});
		}
	}

	return out;
}

MeshIndices MeshIndices::changeType(MeshIndices indices, Type new_type) {
	if(new_type == indices.type())
		return indices;

	if(new_type == Type::triangle_strip)
		THROW("Please write me");
	if(new_type == Type::triangles) {
		auto tris = indices.trisIndices();
		indices.m_data.reserve(tris.size() * 3);
		indices.m_data.clear();
		for(const auto &tri : tris)
			indices.m_data.insert(end(indices.m_data), begin(tri), end(tri));
	}

	return indices;
}

MeshBuffers::MeshBuffers(vector<float3> positions, vector<float3> normals,
						 vector<float2> tex_coords)
	: positions(std::move(positions)), normals(std::move(normals)),
	  tex_coords(std::move(tex_coords)) {
	DASSERT(tex_coords.empty() || positions.size() == tex_coords.size());
	DASSERT(normals.empty() || positions.size() == normals.size());
}

MeshBuffers::MeshBuffers(PVertexBuffer positions, PVertexBuffer normals, PVertexBuffer tex_coords)
	: MeshBuffers((DASSERT(positions), positions->getData<float3>()),
				  normals ? normals->getData<float3>() : vector<float3>(),
				  tex_coords ? tex_coords->getData<float2>() : vector<float2>()) {}

template <class T> static PVertexBuffer extractBuffer(PVertexArray array, int buffer_id) {
	DASSERT(array);
	const auto &sources = array->sources();
	DASSERT(buffer_id == -1 || (buffer_id >= 0 && buffer_id < (int)sources.size()));
	return buffer_id == -1 ? PVertexBuffer() : sources[buffer_id].buffer();
}

MeshBuffers::MeshBuffers(PVertexArray array, int positions_id, int normals_id, int tex_coords_id)
	: MeshBuffers(extractBuffer<float3>(array, positions_id),
				  extractBuffer<float3>(array, normals_id),
				  extractBuffer<float2>(array, tex_coords_id)) {}

MeshBuffers::MeshBuffers(const XMLNode &node)
	: MeshBuffers(node.childValue<vector<float3>>("positions", {}),
				  node.childValue<vector<float3>>("normals", {}),
				  node.childValue<vector<float2>>("tex_coords", {})) {}

void MeshBuffers::saveToXML(XMLNode node) const {
	using namespace xml_conversions;
	node.addChild("positions", node.own(toString(positions)));
	if(!tex_coords.empty())
		node.addChild("tex_coords", node.own(toString(tex_coords)));
	if(!normals.empty())
		node.addChild("normals", node.own(toString(normals)));
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

Mesh::Mesh(MeshBuffers buffers, vector<MeshIndices> indices, vector<MaterialRef> materials)
	: m_buffers(std::move(buffers)), m_indices(std::move(indices)),
	  m_materials(std::move(materials)), m_is_drawing_cache_dirty(true) {
	afterInit();
}

void Mesh::afterInit() {
	for(const auto &indices : m_indices) {
		DASSERT((int)indices.maxIndex() < m_buffers.size());
		DASSERT(indices.type() == primitiveType());
	}
	m_merged_indices = MeshIndices::merge(m_indices, m_merged_ranges);
	computeBoundingBox();
}

static vector<MeshIndices> loadIndices(const XMLNode &node) {
	vector<MeshIndices> out;
	XMLNode xml_indices = node.child("indices");
	while(xml_indices) {
		PrimitiveType::Type type = PrimitiveType::triangles;
		if(const char *type_string = xml_indices.hasAttrib("type"))
			type = PrimitiveType::fromString(type_string);
		out.emplace_back(xml_indices.value<vector<uint>>(), type);
		xml_indices.next();
	}
	return out;
}

static vector<MaterialRef> loadMaterials(const XMLNode &node) {
	XMLNode xml_indices = node.child("indices");
	vector<string> names;
	while(xml_indices) {
		names.emplace_back(xml_indices.attrib("mat_name", ""));
		xml_indices.next();
	}
	return vector<MaterialRef>(begin(names), end(names));
}

Mesh::Mesh(const XMLNode &node) : Mesh(MeshBuffers(node), loadIndices(node), loadMaterials(node)) {}

void Mesh::assignMaterials(const std::map<string, PMaterial> &map) {
	for(auto &mat : m_materials) {
		auto it = map.find(mat.name);
		if(it != map.end())
			mat.pointer = it->second;
	}
}

void Mesh::saveToXML(XMLNode node) const {
	m_buffers.saveToXML(node);
	for(int n = 0; n < (int)m_indices.size(); n++) {
		const auto &indices = m_indices[n];
		string mat_name = m_materials.empty() ? string() : m_materials[n].name;

		XMLNode xml_indices =
			node.addChild("indices", node.own(xml_conversions::toString((vector<uint>)indices)));
		if(indices.type() != PrimitiveType::triangles)
			xml_indices.addAttrib("type", toString(indices.type()));
		if(!mat_name.empty())
			xml_indices.addAttrib("mat_name", xml_indices.own(mat_name));
	}
}

void Mesh::transformUV(const Matrix4 &matrix) {
	auto &tex_coords = m_buffers.tex_coords;
	for(int n = 0; n < (int)tex_coords.size(); n++)
		tex_coords[n] = (matrix * float4(tex_coords[n], 0.0f, 1.0f)).xy();
	m_is_drawing_cache_dirty = true;
}

void Mesh::computeBoundingBox() { m_bounding_box = FBox(m_buffers.positions); }

int Mesh::triangleCount() const {
	int count = 0;
	for(const auto &indices : m_indices)
		count += indices.triangleCount();
	return count;
}

vector<Mesh::TriIndices> Mesh::trisIndices() const {
	vector<TriIndices> out;
	for(const auto &indices : m_indices) {
		auto tris = indices.trisIndices();
		out.insert(end(out), begin(tris), end(tris));
	}
	return out;
}

// TODO: test split / merge and transform
vector<Mesh> Mesh::split(int max_vertices) const {
	THROW("fixme");
	return {*this};
	/*
	DASSERT(max_vertices >= 3 && !m_indices.empty());
	vector<Mesh> out;

	int last_index = 0;
	auto tris_indices = trisIndices();

	while(last_index < (int)tris_indices.size()) {
		out.push_back(Mesh());
		auto &new_mesh = out.back();

		int num_vertices = 0;
		std::unordered_map<uint, uint> index_map;
		vector<pair<uint, uint>> index_list;

		for(int i = last_index; i < (int)tris_indices.size(); i++) {
			if(num_vertices + 3 > max_vertices)
				break;
			const auto &tri = tris_indices[i];
			for(int j = 0; j < 3; j++) {
				uint vindex = tri[j];
				auto it = index_map.find(vindex);
				if(it == index_map.end())
					it = index_map.emplace(vindex, num_vertices++).first;
				index_list.emplace_back(vindex, it->second);
			}
			last_index++;
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
		new_mesh.m_primitive_type = PrimitiveType::triangles;
		new_mesh.computeBoundingBox();
	}

	return out;*/
}

Mesh Mesh::merge(const vector<Mesh> &meshes) {
	// THROW("fixme");
	return meshes.front();
	/*
	Mesh out;

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
	return out;*/
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

Mesh Mesh::transform(const Matrix4 &mat, Mesh mesh) {
	mesh.m_buffers.positions = transformVertices(mat, std::move(mesh.m_buffers.positions));
	if(mesh.hasNormals())
		mesh.m_buffers.normals = transformNormals(mat, std::move(mesh.m_buffers.normals));
	return mesh;
}

void Mesh::draw(Renderer &out, PMaterial material, const Matrix4 &matrix) const {
	if(m_is_drawing_cache_dirty)
		updateDrawingCache();

	for(const auto &cache_elem : m_drawing_cache)
		out.addDrawCall(cache_elem.first,
						cache_elem.second.pointer ? cache_elem.second.pointer : material, matrix);
}

void Mesh::updateDrawingCache() const {
	m_drawing_cache.clear();

	if(m_buffers.positions.size() > IndexBuffer::max_index_value) {
		auto parts = split(IndexBuffer::max_index_value);
		for(auto &part : parts) {
			part.updateDrawingCache();
			m_drawing_cache.insert(end(m_drawing_cache), begin(part.m_drawing_cache),
								   end(part.m_drawing_cache));
		}
	} else {
		auto vertices = make_shared<VertexBuffer>(m_buffers.positions);
		auto tex_coords = hasTexCoords() ? make_shared<VertexBuffer>(m_buffers.tex_coords)
										 : VertexArraySource(float2(0, 0));
		auto indices = make_shared<IndexBuffer>(m_merged_indices);
		auto varray = VertexArray::make({vertices, Color::white, tex_coords}, std::move(indices));

		for(int n = 0; n < (int)m_indices.size(); n++) {
			const auto &range = m_merged_ranges[n];
			const auto &indices = m_indices[n];
			m_drawing_cache.emplace_back(
				DrawCall(varray, indices.type(), range.second, range.first),
				m_materials.empty() ? MaterialRef() : m_materials[n]);
		}
	}
	m_is_drawing_cache_dirty = false;
}

void Mesh::clearDrawingCache() const {
	m_drawing_cache.clear();
	m_is_drawing_cache_dirty = true;
}

float Mesh::intersect(const Segment &segment) const {
	float min_isect = constant::inf;

	const auto &positions = m_buffers.positions;
	if(intersection(segment, m_bounding_box) < constant::inf)
		for(const auto &indices : trisIndices()) {
			Triangle triangle(positions[indices[0]], positions[indices[1]], positions[indices[2]]);
			min_isect = min(min_isect, intersection(segment, triangle));
		}

	return min_isect;
}
}
