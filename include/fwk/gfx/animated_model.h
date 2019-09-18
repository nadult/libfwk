// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/mesh.h"

namespace fwk {

class AnimatedModel {
  public:
	struct MeshData {
		int mesh_id;
		Mesh::AnimatedData anim_data;
		Matrix4 transform;
	};

	// Referenced model has to be alive as long as AnimatedModel
	AnimatedModel(const Model &, const Pose &);
	AnimatedModel(const Model &);

	Mesh toMesh() const;
	FBox boundingBox() const;
	float intersect(const Segment3<float> &) const;

	// Generates DrawCalls suitable for RenderList
	vector<DrawCall> genDrawCalls(const MaterialSet &, const Matrix4 & = Matrix4::identity()) const;

  private:
	const Model &m_model;
	vector<MeshData> m_meshes;
};
}
