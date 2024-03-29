// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/mesh.h"

#include "fwk/gfx/colored_triangle.h"
#include "fwk/gfx/drawing.h"
#include "fwk/io/xml.h"
#include "fwk/math/constants.h"
#include "fwk/math/segment.h"
#include "fwk/math/triangle.h"
#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_buffer_span.h"

namespace fwk {

Mesh::Mesh(MeshBuffers buffers, vector<MeshIndices> indices, vector<string> material_names)
	: m_buffers(std::move(buffers)), m_indices(std::move(indices)),
	  m_material_names(std::move(material_names)) {
	for(const auto &indices : m_indices)
		DASSERT(indices.empty() || indices.indexRange().second < m_buffers.size());
	DASSERT(!m_material_names || m_material_names.size() == m_indices.size());
	m_bounding_box = enclose(m_buffers.positions);
}

Ex<Mesh> Mesh::load(CXmlNode node) {
	vector<MeshIndices> indices;
	auto xml_indices = node.child("indices");
	while(xml_indices) {
		auto type = xml_indices("type", VPrimitiveTopology::triangle_list);
		indices.emplace_back(xml_indices.value<vector<int>>(), type);
		xml_indices.next();
	}

	auto materials = node.childValue<vector<string>>("materials", {});
	EX_CATCH();

	auto buffers = MeshBuffers::load(node);
	if(!buffers)
		return buffers.error();
	return Mesh{std::move(buffers.get()), std::move(indices), std::move(materials)};
}

void Mesh::saveToXML(XmlNode node) const {
	m_buffers.saveToXML(node);
	for(int n = 0; n < m_indices.size(); n++) {
		const auto &indices = m_indices[n];
		XmlNode xml_indices = node.addChild("indices", indices.data());
		if(indices.topology() != VPrimitiveTopology::triangle_list)
			xml_indices("type") = indices.topology();
	}
	if(m_material_names)
		node.addChild("materials", m_material_names);
}

FBox Mesh::boundingBox(const AnimatedData &anim_data) const {
	return anim_data.empty() ? m_bounding_box : anim_data.bounding_box;
}

int Mesh::triangleCount() const {
	if(!hasIndices())
		return vertexCount() / 3;

	int count = 0;
	for(const auto &indices : m_indices)
		count += indices.triangleCount();
	return count;
}

vector<Mesh::TriIndices> Mesh::trisIndices() const {
	vector<TriIndices> out;
	out.reserve(triangleCount());

	if(hasIndices()) {
		for(const auto &indices : m_indices) {
			auto tris = indices.trisIndices();
			out.insert(end(out), begin(tris), end(tris));
		}
	} else {
		for(int n = 0, count = triangleCount(); n < count; n++)
			out.emplace_back(TriIndices{{n * 3 + 0, n * 3 + 1, n * 3 + 2}});
	}
	return out;
}

vector<Triangle3F> Mesh::tris() const {
	vector<Triangle3F> out;
	out.reserve(triangleCount());

	const auto &verts = positions();
	for(const auto &inds : trisIndices())
		out.emplace_back(verts[inds[0]], verts[inds[1]], verts[inds[2]]);
	return out;
}

vector<ColoredTriangle> Mesh::coloredTris(CSpan<IColor> colors) const {
	vector<ColoredTriangle> out;
	out.reserve(triangleCount());

	const auto &verts = positions();
	if(hasIndices()) {
		DASSERT_EQ(colors.size(), m_indices.size());
		int mat_id = 0;
		for(const auto &indices : m_indices) {
			for(auto &inds : indices.trisIndices())
				out.emplace_back(verts[inds[0]], verts[inds[1]], verts[inds[2]], colors[mat_id]);
			mat_id++;
		}
	} else {
		DASSERT(colors);
		for(int n = 0, count = triangleCount(); n < count; n++)
			out.emplace_back(verts[n * 3 + 0], verts[n * 3 + 1], verts[n * 3 + 2], colors[0]);
	}
	return out;
}

// TODO: test split / merge and transform
vector<Mesh> Mesh::split(int max_vertices) const {
	vector<Mesh> out;
	if(!hasIndices())
		FWK_FATAL("Write me, please");

	for(int n = 0; n < m_indices.size(); n++) {
		const auto &sub_indices = m_indices[n];
		vector<vector<int>> mappings;
		vector<MeshIndices> new_indices = sub_indices.split(max_vertices, mappings);

		string mat_name = !m_material_names ? "" : m_material_names[n];
		for(int i = 0; i < new_indices.size(); i++)
			out.emplace_back(
				Mesh(m_buffers.remap(mappings[i]), {std::move(new_indices[i])}, {mat_name}));
		DASSERT(out.back().vertexCount() <= max_vertices);
	}

	return out;
}

Mesh Mesh::merge(vector<Mesh> meshes) {
	// TODO: Merging skinned meshes
	for(const auto &mesh : meshes) {
		if(!mesh.hasIndices())
			FWK_FATAL("Write me");
	}

	if(meshes.size() == 1)
		return std::move(meshes.front());
	if(!meshes)
		return Mesh();

	int num_vertices = 0;
	bool has_tex_coords = meshes ? meshes.front().hasTexCoords() : false;
	bool has_normals = meshes ? meshes.front().hasNormals() : false;

	for(const auto &mesh : meshes) {
		num_vertices += mesh.vertexCount();
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

		for(int n = 0; n < mesh.indices().size(); n++) {
			out_indices.emplace_back(
				MeshIndices::applyOffset(std::move(mesh.indices()[n]), offset));
			out_materials.emplace_back(mesh.materialNames() ? mesh.materialNames()[n] : "");
		}

		offset += mesh.vertexCount();
	}

	return Mesh({std::move(out_positions), std::move(out_normals), std::move(out_tex_coords)},
				std::move(out_indices), std::move(out_materials));
}

Mesh Mesh::transform(const Matrix4 &mat, Mesh mesh) {
	mesh.m_buffers = MeshBuffers::transform(mat, std::move(mesh.m_buffers));
	mesh.m_bounding_box = enclose(mesh.positions());
	return mesh;
}

void Mesh::removeNormals() { m_buffers.normals.clear(); }
void Mesh::removeTexCoords() { m_buffers.tex_coords.clear(); }
void Mesh::removeColors() { m_buffers.colors.clear(); }

void Mesh::removeIndices(CSpan<Pair<string, IColor>> color_map) {
	if(hasIndices()) {
		auto mapping = fwk::merge(trisIndices());
		vector<IColor> colors;

		if(color_map) {
			removeColors();
			colors.resize(mapping.size());

			int idx = 0;
			for(int i = 0; i < m_indices.size(); i++) {
				IColor color = ColorId::white;
				for(auto &pair : color_map)
					if(pair.first == m_material_names[i]) {
						color = pair.second;
						break;
					}

				int size = m_indices[i].size();
				for(int j = 0; j < size; j++)
					colors[idx + j] = color;
				idx += size;
			}
		}

		m_buffers = m_buffers.remap(mapping);
		m_buffers.colors = colors;
		m_indices.clear();
	}
}

Mesh::AnimatedData Mesh::animate(const Pose &pose) const {
	if(!m_buffers.hasSkin())
		return AnimatedData();

	auto mapped_pose = m_buffers.mapPose(pose);
	auto positions = m_buffers.animatePositions(mapped_pose);
	FBox bbox = enclose(positions);
	return AnimatedData{bbox, std::move(positions), m_buffers.animateNormals(mapped_pose)};
}

Mesh Mesh::apply(Mesh mesh, AnimatedData data) {
	if(!data.empty()) {
		mesh.m_buffers.positions = std::move(data.positions);
		mesh.m_buffers.normals = std::move(data.normals);
		mesh.m_bounding_box = data.bounding_box;
	}
	return mesh;
}

vector<float3> Mesh::lines() const {
	vector<float3> lines;
	lines.reserve(triangleCount() * 6);
	for(auto tri : trisIndices()) {
		int inds[6] = {0, 1, 1, 2, 2, 0};
		for(int i : inds)
			lines.emplace_back(m_buffers.positions[tri[i]]);
	}
	return lines;
}

vector<SimpleDrawCall> Mesh::genDrawCalls(VulkanDevice &device, const SimpleMaterialSet &materials,
										  const AnimatedData *anim_data,
										  const Matrix4 &matrix) const {
	FWK_FATAL("writeme");
	vector<SimpleDrawCall> out;
	/*
	if(anim_data) {
		DASSERT(valid(*anim_data));
		if(anim_data->empty())
			anim_data = nullptr;
	}

	CSpan<float3> verts = anim_data ? anim_data->positions : m_buffers.positions;
	auto vao = RenderList::makeVao(verts, m_buffers.colors, m_buffers.tex_coords);
	FBox bbox = anim_data ? anim_data->bounding_box : m_bounding_box;

	if(hasIndices()) {
		vector<Pair<int>> merged_ranges;
		auto merged_indices = MeshIndices::merge(m_indices, merged_ranges);
		vao->setIndices(GlBuffer::make(BufferType::element_array, merged_indices.data()),
						IndexType::uint32);

		for(int n = 0; n < m_indices.size(); n++) {
			const auto &range = merged_ranges[n];
			const auto &indices = m_indices[n];
			string mat_name = m_material_names ? m_material_names[n] : "";
			out.emplace_back(vao, indices.type(), range.second, range.first, materials[mat_name],
							 matrix, encloseTransformed(bbox, matrix));
		}
	} else {
		out.emplace_back(vao, PrimitiveType::triangles, triangleCount() * 3, 0,
						 materials.defaultMat(), matrix, encloseTransformed(bbox, matrix));
	}*/

	return out;
}

float Mesh::intersect(const Segment3<float> &segment) const {
	float min_isect = inf;

	if(segment.isectParam(boundingBox()))
		for(Triangle3F triangle : tris())
			min_isect = min(min_isect, segment.isectParam(triangle).first.closest());

	return min_isect;
}

float Mesh::intersect(const Segment3<float> &segment, const AnimatedData &anim_data) const {
	if(anim_data.empty())
		return intersect(segment);

	DASSERT(valid(anim_data));

	float min_isect = inf;
	if(segment.isectParam(anim_data.bounding_box))
		for(const auto &tri : tris())
			min_isect = min(min_isect, segment.isectParam(tri).first.closest());
	return min_isect;
}

bool Mesh::valid(const AnimatedData &anim_data) const {
	return anim_data.empty() || (anim_data.positions.size() == positions().size() &&
								 anim_data.normals.size() == normals().size());
}
}
