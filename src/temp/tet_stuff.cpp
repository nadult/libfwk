

#include <cork.h>

// TODO: make it exception safe...
CorkTriMesh toCork(const Mesh &mesh) {
	CorkTriMesh out;

	vector<TriIndices> tris = mesh.trisIndices();

	out.n_triangles = tris.size();
	out.n_vertices = mesh.positions().size();
	out.vertices = new float[out.n_vertices * 3];
	out.triangles = new uint[out.n_triangles * 3];
	memcpy(out.vertices, mesh.positions()[0].v, out.n_vertices * sizeof(float3));
	memcpy(out.triangles, &tris[0][0], sizeof(uint) * 3 * out.n_triangles);

	return out;
}

Mesh fromCork(const CorkTriMesh &mesh) {
	vector<float3> positions(mesh.n_vertices);
	for(uint n = 0; n < mesh.n_vertices; n++) {
		const float *v = mesh.vertices + n * 3;
		positions[n] = float3(v[0], v[1], v[2]);
	}

	return Mesh{{positions}, {vector<uint>(mesh.triangles, mesh.triangles + mesh.n_triangles * 3)}};
}

vector<Segment> genBoundaryCork(const TetMesh &mesh1, const TetMesh &mesh2) {
	double time = getTime();
	CorkTriMesh cmesh1 = toCork(mesh1.toMesh());
	CorkTriMesh cmesh2 = toCork(mesh2.toMesh());
	CorkTriMesh cout = {0, 0, 0, 0};
	resolveIntersections(cmesh1, cmesh2, &cout);
	printf("cork isects time: %f msec\n", (getTime() - time) * 1000.0);

	auto imesh = fromCork(cout);
	freeCorkTriMesh(&cout);
	freeCorkTriMesh(&cmesh1);
	freeCorkTriMesh(&cmesh2);

	vector<Segment> out;
	for(const auto &tri : imesh.tris())
		insertBack(out, tri.edges());
	return out;
}

for(int i = 0; i < (int)face_triangulations.size(); i++) {
	vector<Triangle2D> tris;
	vector<float2> points;
	vector<Segment2D> segs;
	const auto &proj = projs[i];

	for(auto &tri : face_triangulations[i].second) {
		Triangle ttri(tri[0]->pos(), tri[1]->pos(), tri[2]->pos());
		tris.emplace_back((proj * ttri).xz());
		for(int j = 0; j < 3; j++)
			points.emplace_back((proj * tri[j]->pos()).xz());
	}
	for(auto edge : loops) {
		auto seg = (proj * edge.segment());
		if(fabs(seg.origin().y) < constant::epsilon && fabs(seg.end().y) < constant::epsilon)
			segs.emplace_back(seg.xz());
	}
	saveSvg(points, segs, tris, i, 1000.0f);
}
