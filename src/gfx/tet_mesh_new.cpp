#include "fwk_gfx.h"
#include <algorithm>

namespace fwk {

struct Thing {};

struct Face {
	Face(const float3 &a, const float3 &b, const float3 &c) : m_tri(a, b, c) {}
	Face(const Triangle &tri) : m_tri(tri) {
		float3 points[3] = {tri.a(), tri.b(), tri.c()};
		m_box = FBox(points);
	}

	float3 operator[](int idx) const { return m_tri[idx]; }
	const auto &triangle() const { return m_tri; }
	bool operator<(const Face &face) const { return m_box.min.y < face.m_box.min.y; }

	Triangle m_tri;
	FBox m_box;
};

pair<vector<Face>, vector<Face>> clipFace(const Face &face, pair<float2, float2> segment) {
	vector<Face> inside;
	vector<Face> outside;

	float3 tpoint[3] = {face.triangle()[0], face.triangle()[1], face.triangle()[2]};

	float2 origin = segment.first;
	float2 dir = segment.second - origin;
	float len = length(dir);
	if(len < constant::epsilon) {
		outside = {face};
		return make_pair(inside, outside);
	}
	dir /= len;
	float2 normal(-dir.y, dir.x);

	int inside_count = 0;
	float tdot[3];
	for(int n = 0; n < 3; n++) {
		tdot[n] = dot((tpoint[n].xz() - origin), normal);
		if(tdot[n] >= constant::epsilon)
			inside_count++;
	}

	if(inside_count == 3) {
		inside = {face};
	} else if(inside_count == 0) { outside = {face}; } else if(inside_count == 2) {
		auto out = clipFace(face, make_pair(segment.second, segment.first));
		return make_pair(out.second, out.first);
	} else { // inside_count == 1
		while(tdot[0] < 0.0f) {
			swap(tpoint[0], tpoint[1]);
			swap(tpoint[1], tpoint[2]);
			swap(tdot[0], tdot[1]);
			swap(tdot[1], tdot[2]);
		}
		// now vertex#0 is inside

		//	printf("Normal: %f %f\nDir: %f %f\n", normal.x, normal.y, dir.x, dir.y);
		float2 spoint[3];
		for(int i = 0; i < 3; i++) {
			float2 rel_point = tpoint[i].xz() - origin;
			spoint[i] = float2(rel_point.x * dir[0] + rel_point.y * dir[1],
							   rel_point.x * normal[0] + rel_point.y * normal[1]);

			//		printf("%f %f -> %f %f\n", rel_point.x, rel_point.y, spoint[i].x, spoint[i].y);
			DASSERT(distance(dir * spoint[i].x + normal * spoint[i].y, rel_point) <
					constant::epsilon);
		}

		float2 edge[2] = {spoint[1] - spoint[0], spoint[2] - spoint[0]};
		float tpos[2] = {-spoint[0].y / edge[0].y, -spoint[0].y / edge[1].y};

		for(int i = 0; i < 2; i++)
			if(isnan(tpos[i]) || tpos[i] <= constant::epsilon || tpos[i] > 1.0f - constant::epsilon)
				return make_pair(inside, outside);
		// printf("%f %f\n", tpos[0], tpos[1]);

		float2 cpoint[2] = {spoint[0] + edge[0] * tpos[0], spoint[0] + edge[1] * tpos[1]};
		for(int i = 0; i < 2; i++)
			cpoint[i] = dir * cpoint[i].x + normal * cpoint[i].y + origin;

		float3 ccpoint[2];
		for(int i = 0; i < 2; i++)
			ccpoint[i] =
				float3(cpoint[i][0], lerp(tpoint[0].y, tpoint[i + 1].y, tpos[i]), cpoint[i][1]);

		inside = {Face(tpoint[0], ccpoint[1], ccpoint[0])};
		outside = {Face(ccpoint[1], tpoint[1], tpoint[2]), Face(ccpoint[1], tpoint[2], ccpoint[0])};
	}

	return make_pair(inside, outside);
}

void draw(Renderer &out, const vector<Face> &faces, float scale) {
	vector<float3> lines;

	for(auto face : faces) {
		const auto &tri = face.triangle();
		float3 center = tri.center();
		float3 normal = tri.normal() * scale;
		float3 side = normalize(tri.a() - center) * scale;

		insertBack(lines, {center, center + normal * 0.5f});
		insertBack(lines, {center + normal * 0.5f, center + normal * 0.4f + side * 0.1f});
		insertBack(lines, {center + normal * 0.5f, center + normal * 0.4f - side * 0.1f});
		insertBack(lines, {tri.a(), tri.b()});
		insertBack(lines, {tri.b(), tri.c()});
		insertBack(lines, {tri.c(), tri.a()});
	}
	auto normal_mat = make_immutable<Material>(Color::blue, Material::flag_ignore_depth);
	out.addLines(lines, normal_mat);
}

static void draw(const Tetrahedron &tet, Renderer &out) {
	auto material = make_immutable<Material>(Color::red, Material::flag_clear_depth);

	Mesh mesh = Mesh::makeTetrahedron(tet);
	mesh.draw(out, material);
}

TetMesh TetMesh::make(CRange<float3> positions, CRange<TriIndices> tri_indices,
					  Renderer &renderer) {
	vector<Tetrahedron> tets;

	vector<Face> faces;

	for(const auto &inds : tri_indices) {
		Triangle tri(positions[inds[0]], positions[inds[1]], positions[inds[2]]);
		if(fabs(tri.normal().y) > constant::epsilon)
			faces.emplace_back(tri);
	}

	std::sort(begin(faces), end(faces));

	//	for(const auto &face : faces)
	//		for(const auto &face2 : faces)
	//			for(int i = 0; i < 3; i++)
	//				clipFace(face, make_pair(face2[i].xz(), face2[(i + 1) % 3].xz()));

	Face first = faces.back();

	vector<Face> inside, outside;
	for(const auto &face : faces) {
		for(int i = 0; i < 3; i++) {
			auto out = clipFace(first, make_pair(face[i].xz(), face[(i + 1) % 3].xz()));
			if(out.first.empty() || out.second.empty())
				continue;

			insertBack(inside, out.first);
			insertBack(outside, out.second);

			insertBack(inside, {first, face});
			break;
		}

		if(!inside.empty())
			break;
	}
	faces = inside;
	insertBack(faces, outside);

	float scale = length(FBox(positions).min - FBox(positions).max) * 0.1f;
	fwk::draw(renderer, faces, scale);
	for(const auto &tet : tets)
		fwk::draw(tet, renderer);

	vector<float3> verts;
	return TetMesh(std::move(verts), std::move(vector<TetIndices>()));
}
}
