/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <limits>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Mesh_triangulation_3.h>
#include <CGAL/Mesh_complex_3_in_triangulation_3.h>
#include <CGAL/Mesh_criteria_3.h>
#include <CGAL/Polyhedral_mesh_domain_3.h>
#include <CGAL/make_mesh_3.h>
#include <CGAL/refine_mesh_3.h>
#include <CGAL/Polyhedron_incremental_builder_3.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef CGAL::Polyhedron_3<K> Polyhedron;
typedef Polyhedron::HalfedgeDS HalfedgeDS;
typedef CGAL::Polyhedral_mesh_domain_3<Polyhedron, K> Mesh_domain;
// Triangulation
#ifdef CGAL_CONCURRENT_MESH_3
typedef CGAL::Mesh_triangulation_3<Mesh_domain,
								   CGAL::Kernel_traits<Mesh_domain>::Kernel, // Same as sequential
								   CGAL::Parallel_tag // Tag to activate parallelism
								   >::type Tr;
#else
typedef CGAL::Mesh_triangulation_3<Mesh_domain>::type Tr;
#endif
typedef CGAL::Mesh_complex_3_in_triangulation_3<Tr> C3t3;
// Criteria
typedef CGAL::Mesh_criteria_3<Tr> Mesh_criteria;
// To avoid verbose function and named parameters call
using namespace CGAL::parameters;

namespace fwk {

using TriIndices = TetMesh::TriIndices;

TetMesh::TetMesh(vector<float3> positions, vector<TetIndices> tets)
	: m_positions(std::move(positions)), m_indices(std::move(tets)) {
	for(const auto &tet : m_indices)
		for(auto i : tet)
			DASSERT(i >= 0 && i < (int)m_positions.size());
}

template <class HDS> class Builder : public CGAL::Modifier_base<HDS> {
  public:
	Builder(CRange<float3> positions, CRange<TriIndices> indices)
		: m_positions(positions), m_tris(indices) {}
	void operator()(HDS &hds) {
		// Postcondition: hds is a valid polyhedral surface.
		CGAL::Polyhedron_incremental_builder_3<HDS> builder(hds, true);

		typedef typename HDS::Vertex Vertex;
		typedef typename Vertex::Point Point;

		builder.begin_surface(m_positions.size(), m_tris.size());
		for(const auto &pos : m_positions)
			builder.add_vertex(Point(pos.x, pos.y, pos.z));
		for(const auto &tri : m_tris) {
			builder.begin_facet();
			for(int i = 2; i >= 0; i--)
				builder.add_vertex_to_facet(tri[i]);
			builder.end_facet();
		}
		builder.end_surface();
	}

  private:
	CRange<float3> m_positions;
	CRange<TriIndices> m_tris;
};

TetMesh TetMesh::make(CRange<float3> positions, CRange<TriIndices> faces, Params params) {
	vector<float3> verts;
	vector<TetIndices> tets;
	Polyhedron poly;

	Builder<HalfedgeDS> triangle(positions, faces);
	poly.delegate(triangle);

	Mesh_domain domain(poly);

	Mesh_criteria criteria(facet_angle = params.facet_angle, facet_size = params.facet_size,
						   facet_distance = params.facet_distance, cell_radius_edge_ratio = 3);
	C3t3 c3t3 = CGAL::make_mesh_3<C3t3>(domain, criteria);

	using VertexHandle = C3t3::Vertex_handle;
	std::map<VertexHandle, int> verts_map;

	const auto &tri = c3t3.triangulation();
	for(auto cell = c3t3.cells_in_complex_begin(); cell != c3t3.cells_in_complex_end(); ++cell) {
		TetIndices indices;
		for(int i = 0; i < 4; i++) {
			auto handle = cell->vertex(i);
			auto it = verts_map.find(handle);
			if(it == verts_map.end()) {
				auto point = tri.point(handle);
				it = verts_map.insert(make_pair(handle, (int)verts.size())).first;
				verts.emplace_back(CGAL::to_double(point.x()), CGAL::to_double(point.y()),
								   CGAL::to_double(point.z()));
			}
			indices[i] = it->second;
		}
		tets.emplace_back(indices);
	}

	return TetMesh(std::move(verts), std::move(tets));
}
}
