/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <limits>

namespace fwk {

Mesh::Mesh(MeshBuffers buffers, vector<MeshIndices> indices, vector<string> material_names)
	: m_buffers(std::move(buffers)), m_indices(std::move(indices)),
	  m_material_names(std::move(material_names)), m_is_drawing_cache_dirty(true) {
	afterInit();
}

void Mesh::afterInit() {
	for(const auto &indices : m_indices)
		DASSERT((int)indices.indexRange().second < m_buffers.size());
	DASSERT(m_material_names.empty() || m_material_names.size() == m_indices.size());
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

Mesh::Mesh(const XMLNode &node)
	: Mesh(MeshBuffers(node), loadIndices(node), node.childValue<vector<string>>("materials", {})) {
}

void Mesh::saveToXML(XMLNode node) const {
	m_buffers.saveToXML(node);
	for(int n = 0; n < (int)m_indices.size(); n++) {
		const auto &indices = m_indices[n];
		XMLNode xml_indices = node.addChild("indices", (vector<uint>)indices);
		if(indices.type() != PrimitiveType::triangles)
			xml_indices.addAttrib("type", toString(indices.type()));
	}
	if(!m_material_names.empty())
		node.addChild("materials", m_material_names);
}

void Mesh::transformUV(const Matrix4 &matrix) {
	auto &tex_coords = m_buffers.tex_coords;
	for(int n = 0; n < (int)tex_coords.size(); n++)
		tex_coords[n] = (matrix * float4(tex_coords[n], 0.0f, 1.0f)).xy();
	m_is_drawing_cache_dirty = true;
}

void Mesh::computeBoundingBox() { m_bounding_box = FBox(m_buffers.positions); }

FBox Mesh::boundingBox(const Pose &pose) const {
	if(!m_buffers.hasSkin())
		return boundingBox();

	vector<float3> positions(vertexCount());
	m_buffers.animatePositions(positions, m_buffers.mapPose(pose));
	return FBox(positions);
}

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
	vector<Mesh> out;

	for(int n = 0; n < (int)m_indices.size(); n++) {
		const auto &sub_indices = m_indices[n];
		vector<vector<uint>> mappings;
		vector<MeshIndices> new_indices = sub_indices.split(max_vertices, mappings);

		string mat_name = m_material_names.empty() ? "" : m_material_names[n];
		for(int i = 0; i < (int)new_indices.size(); i++)
			out.emplace_back(
				Mesh(m_buffers.remap(mappings[i]), {std::move(new_indices[i])}, {mat_name}));
		DASSERT(out.back().vertexCount() <= max_vertices);
	}

	return out;
}

Mesh Mesh::merge(vector<Mesh> meshes) {
	for(const auto &mesh : meshes)
		if(mesh.hasSkin())
			THROW("writeme");

	if(meshes.size() == 1)
		return std::move(meshes.front());
	if(meshes.empty())
		return Mesh();

	int num_vertices = 0;
	bool has_tex_coords = meshes.empty() ? false : meshes.front().hasTexCoords();
	bool has_normals = meshes.empty() ? false : meshes.front().hasNormals();

	for(const auto &mesh : meshes) {
		num_vertices += (int)mesh.vertexCount();
		DASSERT(has_tex_coords == mesh.hasTexCoords());
		DASSERT(has_normals == mesh.hasNormals());
	}

	vector<float3> out_positions(num_vertices);
	vector<float2> out_tex_coords(has_tex_coords ? num_vertices : 0);
	vector<float3> out_normals(has_normals ? num_vertices : 0);
	vector<MeshIndices> out_indices;
	vector<string> out_materials;

	int offset = 0;

	for(auto &mesh : meshes) {
		if(mesh.vertexCount() == 0)
			continue;

		std::copy(begin(mesh.positions()), end(mesh.positions()), begin(out_positions) + offset);
		if(has_tex_coords)
			std::copy(begin(mesh.texCoords()), end(mesh.texCoords()),
					  begin(out_tex_coords) + offset);
		if(has_normals)
			std::copy(begin(mesh.normals()), end(mesh.normals()), begin(out_normals) + offset);

		for(int n = 0; n < (int)mesh.indices().size(); n++) {
			out_indices.emplace_back(
				MeshIndices::applyOffset(std::move(mesh.indices()[n]), offset));
			out_materials.emplace_back(mesh.materialNames().empty() ? "" : mesh.materialNames()[n]);
		}

		offset += mesh.vertexCount();
	}

	return Mesh({std::move(out_positions), std::move(out_normals), std::move(out_tex_coords)},
				std::move(out_indices), std::move(out_materials));
}

Mesh Mesh::transform(const Matrix4 &mat, Mesh mesh) {
	mesh.m_buffers = MeshBuffers::transform(mat, std::move(mesh.m_buffers));
	return mesh;
}

Mesh Mesh::animate(const Pose &pose) const {
	if(!m_buffers.hasSkin())
		return *this;

	MeshBuffers new_buffers = m_buffers;
	auto mapped_pose = m_buffers.mapPose(pose);
	m_buffers.animatePositions(new_buffers.positions, mapped_pose);
	if(!new_buffers.normals.empty())
		m_buffers.animateNormals(new_buffers.normals, mapped_pose);
	return Mesh(std::move(new_buffers), m_indices, m_material_names);
}

void Mesh::draw(Renderer &out, const MaterialSet &materials, const Matrix4 &matrix) const {
	if(m_is_drawing_cache_dirty)
		updateDrawingCache();

	for(const auto &cache_elem : m_drawing_cache)
		out.addDrawCall(cache_elem.first, materials[cache_elem.second], matrix);
}

void Mesh::draw(Renderer &out, const Pose &pose, const MaterialSet &materials,
				const Matrix4 &matrix) const {
	if(!m_buffers.hasSkin())
		return draw(out, materials, matrix);
	animate(pose).draw(out, materials, matrix);
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
		auto vertices = make_cow<VertexBuffer>(m_buffers.positions);
		auto tex_coords = hasTexCoords() ? make_cow<VertexBuffer>(m_buffers.tex_coords)
										 : VertexArraySource(float2(0, 0));
		auto indices = make_cow<IndexBuffer>(m_merged_indices);
		auto varray = VertexArray::make({vertices, Color::white, tex_coords}, std::move(indices));

		for(int n = 0; n < (int)m_indices.size(); n++) {
			const auto &range = m_merged_ranges[n];
			const auto &indices = m_indices[n];
			string mat_name = m_material_names.empty() ? "" : m_material_names[n];
			m_drawing_cache.emplace_back(
				DrawCall(varray, indices.type(), range.second, range.first), mat_name);
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

float Mesh::intersect(const Segment &segment, const Pose &pose) const {
	if(!m_buffers.hasSkin())
		return intersect(segment);

	vector<float3> positions(vertexCount());
	m_buffers.animatePositions(positions, m_buffers.mapPose(pose));

	float min_isect = constant::inf;
	// TODO: optimize
	if(intersection(segment, FBox(positions)) < constant::inf)
		for(const auto &tri : trisIndices()) {
			float isect = intersection(
				segment, Triangle(positions[tri[0]], positions[tri[1]], positions[tri[2]]));
			min_isect = min(min_isect, isect);
		}
	return min_isect;
}
}
