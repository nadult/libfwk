/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#pragma once

#include "fwk_gfx.h"

namespace fwk {

class TetMesh : public immutable_base<TetMesh> {
  public:
	using TriIndices = MeshIndices::TriIndices;
	using TetIndices = array<int, 4>;

	TetMesh(vector<float3> positions = vector<float3>(),
			CRange<TetIndices> tet_verts = CRange<TetIndices>());
	static TetMesh makeTetSoup(CRange<Tetrahedron>);

	array<int, 3> tetFace(int tet, int face_id) const {
		DASSERT(tet >= 0 && tet < (int)m_tet_tets.size());
		DASSERT(face_id >= 0 && face_id < 4);
		static int inds[4][3] = {{0, 1, 2}, {1, 3, 2}, {2, 3, 0}, {3, 1, 0}};
		const auto &tverts = m_tet_verts[tet];
		return {{tverts[inds[face_id][0]], tverts[inds[face_id][1]], tverts[inds[face_id][2]]}};
	}
	Triangle tetTri(int tet, int face_id) const {
		auto face = tetFace(tet, face_id);
		return Triangle(m_verts[face[0]], m_verts[face[1]], m_verts[face[2]]);
	}
	int tetTet(int tet, int face_id) const {
		DASSERT(tet >= 0 && tet < (int)m_tet_tets.size());
		DASSERT(face_id >= 0 && face_id < 4);
		return m_tet_tets[tet][face_id];
	}
	Tetrahedron makeTet(int tet) const;

	enum Flags {
		flag_quality = 1,
		flag_print_details = 2,
	};

	static TetMesh transform(const Matrix4 &, TetMesh);
	static Mesh findIntersections(const Mesh &);
	static TetMesh make(const Mesh &, uint flags = 0);
	static TetMesh merge(const vector<TetMesh> &);
	static TetMesh selectTets(const TetMesh &, const vector<int> &indices);

	TetMesh extract(CRange<int> tet_indices) const;
	vector<int> selection(const FBox &) const;
	vector<int> invertSelection(CRange<int>) const;
	bool isValidSelection(CRange<int>) const;

	vector<Tetrahedron> tets() const;

	enum CSGMode {
		csg_difference,
		csg_intersection,
		csg_union,
	};

	struct CSGVisualData {
		CSGVisualData() : max_steps(0), phase(0) {}

		vector<pair<Color, vector<Triangle>>> poly_soups;
		vector<pair<Color, vector<Segment>>> segment_groups;
		vector<pair<Color, vector<Segment>>> segment_groups_trans;
		vector<pair<Color, TetMesh>> tet_meshes;
		vector<pair<Color, vector<float3>>> point_sets;
		int max_steps, phase;
		enum { max_phases = 6 };
	};

	static TetMesh csg(const TetMesh &, const TetMesh &, CSGMode mode,
					   CSGVisualData *vis_data = nullptr);

	void drawLines(Renderer &, PMaterial, const Matrix4 &matrix = Matrix4::identity()) const;
	void drawTets(Renderer &, PMaterial, const Matrix4 &matrix = Matrix4::identity()) const;

	const auto &verts() const { return m_verts; }
	const auto &tetVerts() const { return m_tet_verts; }
	const auto &tetTets() const { return m_tet_tets; }
	int size() const { return (int)m_tet_tets.size(); }

	Mesh toMesh() const;
	FBox computeBBox() const;

  private:
	vector<float3> m_verts;
	vector<array<int, 4>> m_tet_tets;
	vector<array<int, 4>> m_tet_verts;
};

using PTetMesh = immutable_ptr<TetMesh>;
