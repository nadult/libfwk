// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/animated_model.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/model.h"
#include "fwk/gfx/pose.h"
#include "fwk/math/constants.h"
#include "fwk/math/segment.h"

namespace fwk {

AnimatedModel::AnimatedModel(vector<MeshData> data) : m_meshes(move(data)) {}

AnimatedModel::AnimatedModel(PModel pmodel, PPose pose) : m_model(pmodel) {
	auto &model = *pmodel;
	if(!pose)
		pose = model.defaultPose();
	DASSERT(model.valid(pose));

	m_meshes.reserve(model.nodes().size());
	auto global_pose = model.globalPose(pose);
	const auto &transforms = global_pose->transforms();

	for(auto &node : model.nodes())
		if(auto *mesh = model.mesh(node.mesh_id)) {
			Mesh::AnimatedData anim_data;
			if(mesh->hasSkin()) {
				auto skinning_pose = model.meshSkinningPose(global_pose, node.id);
				anim_data = mesh->animate(skinning_pose);
			}

			m_meshes.emplace_back(node.mesh_id, move(anim_data), transforms[node.id]);
		}
}

Mesh AnimatedModel::toMesh() const {
	vector<Mesh> meshes;
	for(auto mesh_data : m_meshes) {
		auto anim_mesh = Mesh::apply(*m_model->mesh(mesh_data.mesh_id), mesh_data.anim_data);
		meshes.emplace_back(Mesh::transform(mesh_data.transform, anim_mesh));
	}
	return Mesh::merge(meshes);
}

float AnimatedModel::intersect(const Segment3<float> &segment) const {
	float min_isect = inf;

	for(auto mesh_data : m_meshes) {
		auto inv_segment = inverseOrZero(mesh_data.transform) * segment;
		auto *mesh = m_model->mesh(mesh_data.mesh_id);
		float inv_isect = mesh->intersect(inv_segment, mesh_data.anim_data);
		if(inv_isect < inf) {
			float3 point = mulPoint(mesh_data.transform, inv_segment.at(inv_isect));
			min_isect = min(min_isect, distance(segment.from, point));
		}
	}

	return min_isect;
}
FBox AnimatedModel::boundingBox() const {
	FBox out;
	for(auto &mesh_data : m_meshes) {
		auto *mesh = m_model->mesh(mesh_data.mesh_id);
		FBox bbox = encloseTransformed(mesh->boundingBox(mesh_data.anim_data), mesh_data.transform);
		out = encloseNotEmpty(out, bbox);
	}

	return out;
}

vector<DrawCall> AnimatedModel::genDrawCalls(const MaterialSet &materials,
											 const Matrix4 &matrix) const {
	vector<DrawCall> out;
	out.reserve(m_meshes.size());
	for(auto &mesh_data : m_meshes) {
		auto mat = matrix * mesh_data.transform;
		auto *mesh = m_model->mesh(mesh_data.mesh_id);
		insertBack(out, mesh->genDrawCalls(materials, &mesh_data.anim_data, mat));
	}

	return out;
}
}
