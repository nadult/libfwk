// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_mesh.h"

namespace fwk {

AnimatedModel::AnimatedModel(vector<MeshData> data) : m_meshes(move(data)) {}

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

			m_meshes.emplace_back(MeshData{node->mesh(), move(anim_data), transforms[node->id()]});
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

float AnimatedModel::intersect(const Segment3<float> &segment) const {
	float min_isect = fconstant::inf;

	for(auto mesh_data : m_meshes) {
		auto inv_segment = inverse(mesh_data.transform) * segment;
		float inv_isect = mesh_data.mesh->intersect(inv_segment, mesh_data.anim_data);
		if(inv_isect < fconstant::inf) {
			float3 point = mulPoint(mesh_data.transform, inv_segment.at(inv_isect));
			min_isect = min(min_isect, distance(segment.from, point));
		}
	}

	return min_isect;
}
FBox AnimatedModel::boundingBox() const {
	FBox out;
	for(auto mesh_data : m_meshes) {
		FBox bbox = encloseTransformed(mesh_data.mesh->boundingBox(mesh_data.anim_data),
									   mesh_data.transform);
		out = encloseNotEmpty(out, bbox);
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
