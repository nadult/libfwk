// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/mesh_buffers.h"
#include "fwk/gfx/mesh_indices.h"
#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"

namespace fwk {

class Mesh : public immutable_base<Mesh> {
  public:
	Mesh(MeshBuffers buffers = MeshBuffers(), vector<MeshIndices> indices = {},
		 vector<string> mat_names = {});
	Mesh(const Mesh &) = default;
	Mesh(Mesh &&) = default;
	Mesh &operator=(Mesh &&) = default;
	Mesh &operator=(const Mesh &) = default;

	Mesh(CXmlNode);
	void saveToXML(XmlNode) const;

	static Mesh makePolySoup(CSpan<Triangle3F>);
	static Mesh makeRect(const FRect &xz_rect, float y);
	static Mesh makeBBox(const FBox &bbox);
	static Mesh makeCylinder(const Cylinder &, int num_sides);
	static Mesh makeTetrahedron(const Tetrahedron &);
	static Mesh makePlane(const Plane3F &, const float3 &start, float extent);

	struct AnimatedData {
		bool empty() const { return positions.empty(); }

		FBox bounding_box;
		vector<float3> positions;
		vector<float3> normals;
	};

	FBox boundingBox() const { return m_bounding_box; }
	FBox boundingBox(const AnimatedData &) const;

	void changePrimitiveType(PrimitiveType new_type);

	int vertexCount() const { return (int)m_buffers.positions.size(); }
	int triangleCount() const;

	const MeshBuffers &buffers() const { return m_buffers; }
	const vector<float3> &positions() const { return m_buffers.positions; }
	const vector<float3> &normals() const { return m_buffers.normals; }
	const vector<float2> &texCoords() const { return m_buffers.tex_coords; }
	const vector<MeshIndices> &indices() const { return m_indices; }
	const auto &materialNames() const { return m_material_names; }

	bool hasTexCoords() const { return !m_buffers.tex_coords.empty(); }
	bool hasNormals() const { return !m_buffers.normals.empty(); }
	bool hasColors() const { return !m_buffers.colors.empty(); }
	bool hasSkin() const { return m_buffers.hasSkin(); }
	bool hasIndices() const { return !m_indices.empty(); }
	bool empty() const { return m_buffers.positions.empty(); }

	void removeNormals();
	void removeTexCoords();
	void removeColors();
	void removeIndices(CSpan<pair<string, IColor>> color_map = {});
	static Mesh transform(const Matrix4 &, Mesh);

	using TriIndices = MeshIndices::TriIndices;
	vector<Triangle3F> tris() const;
	vector<TriIndices> trisIndices() const;

	vector<Mesh> split(int max_vertices) const;
	static Mesh merge(vector<Mesh>);

	float intersect(const Segment3<float> &) const;
	float intersect(const Segment3<float> &, const AnimatedData &) const;

	AnimatedData animate(PPose) const;
	static Mesh apply(Mesh, AnimatedData);

	vector<float3> lines() const;
	vector<DrawCall> genDrawCalls(const MaterialSet &, const AnimatedData *anim_data = nullptr,
								  const Matrix4 & = Matrix4::identity()) const;

  protected:
	bool valid(const AnimatedData &) const;

	MeshBuffers m_buffers;
	vector<MeshIndices> m_indices;
	vector<string> m_material_names;
	FBox m_bounding_box;
};
}
