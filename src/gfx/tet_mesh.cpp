/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include "fwk_profile.h"
#include <algorithm>
#include <limits>

namespace fwk {

using TriIndices = TetMesh::TriIndices;

TetMesh::TetMesh(vector<float3> positions, vector<TetIndices> tets)
	: m_positions(std::move(positions)), m_indices(std::move(tets)) {
	for(const auto &tet : m_indices)
		for(auto i : tet)
			DASSERT(i >= 0 && i < (int)m_positions.size());
}

void TetMesh::draw(Renderer &out, PMaterial material, const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	vector<float3> lines;

	for(auto tet : m_indices) {
		float3 tlines[12];
		int pairs[12] = {0, 1, 0, 2, 2, 1, 3, 0, 3, 1, 3, 2};
		for(int i = 0; i < arraySize(pairs); i++)
			tlines[i] = m_positions[tet[pairs[i]]];
		lines.insert(end(lines), begin(tlines), end(tlines));
	}
	out.addLines(lines, material);

	out.popViewMatrix();
}

TetMesh TetMesh::makeUnion(const vector<TetMesh> &sub_tets) {
	vector<float3> positions;
	vector<TetIndices> indices;

	for(const auto &sub_tet : sub_tets) {
		int off = (int)positions.size();
		insertBack(positions, sub_tet.m_positions);
		for(auto tidx : sub_tet.m_indices) {
			TetIndices inds{{tidx[0] + off, tidx[1] + off, tidx[2] + off, tidx[3] + off}};
			indices.emplace_back(inds);
		}
	}

	return TetMesh(std::move(positions), std::move(indices));
}
}
