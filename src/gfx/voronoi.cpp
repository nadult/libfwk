#include "fwk_gfx.h"
#include "voropp.hh"

namespace fwk {

Voronoi::Voronoi(vector<float3> points) : m_points(points), m_bbox(points) {}

Mesh Voronoi::toMesh() const {
	double time = getTime();

	FBox bbox = enlarge(m_bbox, 1.0f);
	int3 grid_size(26, 26, 26);
	voro::container con(bbox.min.x, bbox.max.x, bbox.min.y, bbox.max.y, bbox.min.z, bbox.max.z,
						grid_size.x, grid_size.y, grid_size.y, false, false, false, 8);

	for(int n = 0; n < (int)m_points.size(); n++) {
		const auto &point = m_points[n];
		con.put(n, point.x, point.y, point.z);
	}

	voro::voronoicell_neighbor c;
	voro::c_loop_all cl(con);
	std::vector<int> neigh, f_vert;
	std::vector<double> v;
	int id, nx, ny, nz;

	vector<Triangle> tris;
	vector<char> is_outside(m_points.size(), false);

	if(cl.start())
		do
			if(con.compute_cell(c, cl)) {
				double x, y, z;

				cl.pos(x, y, z);
				id = cl.pid();

				// Gather information about the computed Voronoi cell
				c.neighbors(neigh);
				c.face_vertices(f_vert);
				c.vertices(x, y, z, v);

				is_outside[id] = anyOf(neigh, [](int i) { return i < 0; });
				if(!is_outside[id])
					for(int i = 0, j = 0; i < (int)neigh.size(); i++) {
						if(1) {
							auto pair = make_pair(id, neigh[i]);

							vector<float3> verts;
							for(int k = 0; k < f_vert[j]; k++) {
								int l = 3 * f_vert[j + k + 1];
								verts.emplace_back(v[l], v[l + 1], v[l + 2]);
							}
							for(int i = 2; i < (int)verts.size(); i++)
								tris.emplace_back(verts[0], verts[i - 1], verts[i]);
						}

						j += f_vert[j] + 1;
					}
			}
		while(cl.inc());

	time = getTime() - time;
	printf("Voronoi::toMesh(%d points): %.2f msec\n", (int)m_points.size(), time * 1000.0);
	return Mesh::makePolySoup(tris);
}

DynamicMesh DynamicMesh::csgDifference(DynamicMesh a, DynamicMesh b, CSGVisualData *data) {
	srand(0);
	vector<float3> points;
	for(int n = 0; n < 100; n++)
		points.emplace_back(frand() * 10.0f - 5.0f, frand() * 10.0f - 5.0f, frand() * 10.0f - 5.0f);
	auto mesh = Voronoi(points).toMesh();
	auto tris = mesh.tris();

	if(data && data->phase == 0) {
		vector<Triangle> flipped = transform(tris, [](auto tri) { return tri.inverse(); });
		vector<Segment> segs;
		for(auto tri : tris)
			for(auto edge : tri.edges())
				segs.emplace_back(edge);
		data->poly_soups.emplace_back(Color::red, tris);
		data->poly_soups.emplace_back(Color::red, flipped);
		// data->segment_groups.emplace_back(Color::black, segs);
	}

	return {};
}
}
