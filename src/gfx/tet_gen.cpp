#include "fwk_gfx.h"
#include <algorithm>

#define TETLIBRARY
#include "tetgen/tetgen.h"
#include "tetgen/predicates.cxx"
#include "tetgen/tetgen.cxx"

namespace fwk {

using TriIndices = MeshIndices::TriIndices;

namespace {
	void tetgen(const string &opts, CRange<float3> verts, CRange<TriIndices> tris, tetgenio &out) {
		tetgenio in;

		vector<double> points(verts.size() * 3);
		for(int n = 0; n < (int)verts.size(); n++)
			for(int i = 0; i < 3; i++)
				points[n * 3 + i] = verts[n][i];

		vector<int> poly_indices(tris.size() * 3);
		vector<tetgenio::polygon> polygons(tris.size());
		vector<tetgenio::facet> facets(tris.size());
		vector<int> facet_markers(tris.size());
		std::iota(begin(facet_markers), end(facet_markers), 0);

		for(int n = 0; n < (int)tris.size(); n++) {
			for(int i = 0; i < 3; i++)
				poly_indices[n * 3 + i] = tris[n][i];
			polygons[n].vertexlist = &poly_indices[n * 3];
			polygons[n].numberofvertices = 3;
			facets[n].polygonlist = &polygons[n];
			facets[n].numberofpolygons = 1;
			facets[n].numberofholes = 0;
			facets[n].holelist = nullptr;
		}

		in.mesh_dim = 3;
		in.numberofpoints = verts.size();
		in.pointlist = points.data();
		in.numberoffacets = facets.size();
		in.facetlist = facets.data();
		in.facetmarkerlist = facet_markers.data();

		char options[256];
		snprintf(options, arraySize(options), "%s", opts.c_str());

		try {
			tetrahedralize(options, &in, &out, nullptr, nullptr);
		} catch(int error) {
			in.initialize();
			pair<int, const char *> error_code[] = {{1, "error while allocating memory"},
													{2, "internal error"},
													{3, "self intersection detected"},
													{4, "very small input feature size detected"},
													{5, "very close input facets detected"},
													{10, "input error"}};

			for(auto code : error_code)
				if(error == code.first)
					THROW("tetgen: %s", code.second);
			THROW("tetgen: unknown error");
		}
		in.initialize();
	}
}

Mesh TetMesh::findIntersections(const Mesh &mesh) {
	tetgenio out;
	tetgen("dQ", mesh.positions(), mesh.trisIndices(), out);

	vector<TriIndices> inds(out.numberoftrifaces);
	for(int n = 0; n < out.numberoftrifaces; n++) {
		auto *tri = out.trifacelist + n * 3;
		inds[n] = {{(uint)tri[0], (uint)tri[1], (uint)tri[2]}};
	}

	return Mesh(mesh.buffers(), {std::move(inds)});
}

TetMesh TetMesh::make(const Mesh &mesh, uint flags) {
	DASSERT(HalfMesh(mesh).is2Manifold());
	auto opts =
		string("p") + (flags & flag_quality ? "q" : "") + (flags & flag_print_details ? "V" : "Q");

	tetgenio out;
	tetgen(opts, mesh.positions(), mesh.trisIndices(), out);
	DASSERT(out.numberofcorners == 4);

	vector<float3> tet_points(out.numberofpoints);
	vector<TetMesh::TetIndices> tet_indices(out.numberoftetrahedra);

	for(int n = 0; n < out.numberofpoints; n++) {
		auto *point = out.pointlist + n * 3;
		tet_points[n] = float3(point[0], point[1], point[2]);
	}
	for(int n = 0; n < out.numberoftetrahedra; n++) {
		auto *tet = out.tetrahedronlist + n * out.numberofcorners;
		tet_indices[n] = {{tet[0], tet[1], tet[2], tet[3]}};
	}

	return TetMesh(std::move(tet_points), std::move(tet_indices));
}
}
