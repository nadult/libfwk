/* Copyright (C) 2015 - 2016 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_mesh.h"

namespace fwk {

AnimatedModel::AnimatedModel(vector<MeshData> data) : m_meshes(std::move(data)) {}

AnimatedModel::AnimatedModel(const Model &model, PPose pose) {
	if(!pose)
		pose = model.defaultPose();
	DASSERT(model.valid(pose));

	m_meshes.reserve(model.nodes().size());
	auto global_pose = model.globalPose(pose);
	const auto &transforms = global_pose->transforms();

	for(const auto *node : model.nodes())
		if(node->mesh()) {
			Mesh::AnimatedData anim_data;
			if(node->mesh()->hasSkin()) {
				auto skinning_pose = model.meshSkinningPose(global_pose, node->id());
				anim_data = node->mesh()->animate(skinning_pose);
			}

			m_meshes.emplace_back(
				MeshData{node->mesh(), std::move(anim_data), transforms[node->id()]});
		}
}

Mesh AnimatedModel::toMesh() const {
	vector<Mesh> meshes;
	for(auto mesh_data : m_meshes) {
		auto anim_mesh = Mesh::apply(*mesh_data.mesh, mesh_data.anim_data);
		meshes.emplace_back(Mesh::transform(mesh_data.transform, anim_mesh));
	}
	return Mesh::merge(meshes);
}

float AnimatedModel::intersect(const Segment &segment) const {
	float min_isect = constant::inf;

	for(auto mesh_data : m_meshes) {
		auto inv_segment = inverse(mesh_data.transform) * segment;
		float inv_isect = mesh_data.mesh->intersect(inv_segment, mesh_data.anim_data);
		if(inv_isect < constant::inf) {
			float3 point = mulPoint(mesh_data.transform, inv_segment.at(inv_isect));
			min_isect = min(min_isect, distance(segment.origin(), point));
		}
	}

	return min_isect;
}
FBox AnimatedModel::boundingBox() const {
	FBox out;
	for(auto mesh_data : m_meshes) {
		FBox bbox = mesh_data.transform * mesh_data.mesh->boundingBox(mesh_data.anim_data);
		out = out.empty() ? bbox : sum(out, bbox);
	}

	return out;
}

vector<DrawCall> AnimatedModel::genDrawCalls(const MaterialSet &materials,
											 const Matrix4 &matrix) const {
	vector<DrawCall> out;
	out.reserve(m_meshes.size());
	for(auto data : m_meshes) {
		auto mat = matrix * data.transform;
		insertBack(out, data.mesh->genDrawCalls(materials, &data.anim_data, mat));
	}

	return out;
}
}
