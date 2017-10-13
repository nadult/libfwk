// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/filesystem.h"
#include "fwk_mesh.h"
#include "testing.h"
#include <tuple>

string mainPath(string file_name) {
	FilePath exec(executablePath());
	return exec.parent().parent() / file_name;
}

void testSplittingMerging() {
	auto cylinder = Mesh::makeCylinder(Cylinder({0, 0, 0}, 1, 2), 32);
	vector<Mesh> parts;
	for(int x = 0; x < 32; x++)
		for(int y = 0; y < 32; y++)
			parts.emplace_back(Mesh::transform(translation(x * 2, 0, y * 2), cylinder));
	Mesh big_mesh = Mesh::merge(parts);

	parts = big_mesh.split(1024);
	for(auto &part : parts)
		ASSERT(part.vertexCount() <= 1024);

	Mesh merged = Mesh::merge(parts);
	ASSERT(merged.triangleCount() == big_mesh.triangleCount());
	// TODO: test triangle strips as well
}

void testMain() {
#ifndef FWK_TARGET_LINUX
	printf("TODO: models test not supported on mingw\n");
	return;
#endif

	string mesh_path = mainPath("test/test.model");
	auto command =
		format("% % %", mainPath("tools/model_convert"), mainPath("data/test.blend"), mesh_path);
	execCommand(command);
	XMLDocument doc;
	Loader(mesh_path) >> doc;
	PModel model = PModel(Model::loadFromXML(doc.child()));
	remove(mesh_path.c_str());

	auto tmesh = AnimatedModel(*model, model->defaultPose()).toMesh();
	auto tmesh_soup = Mesh::makePolySoup(tmesh.tris());
	ASSERT(tmesh.triangleCount() == tmesh_soup.triangleCount());

	int cube_id = model->findNodeId("cube");
	int plane_id = model->findNodeId("plane");
	int cone_id = model->findNodeId("cone");

	ASSERT(cube_id != -1 && plane_id != -1 && cone_id != -1);
	const auto &nodes = model->nodes();
	ASSERT(nodes[plane_id]->parent()->id() == cube_id);

	auto pose = model->globalPose(model->defaultPose());
	vector<AffineTrans> transforms(begin(pose->transforms()), end(pose->transforms()));

	assertCloseEnough(transforms[cube_id].translation, float3(10, 0, 0));
	assertCloseEnough(transforms[plane_id].translation, float3(0, 0, -5));
	assertCloseEnough(transforms[cone_id].translation, float3(0, -3, 0));
}
