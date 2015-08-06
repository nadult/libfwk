/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "testing.h"
#include "fwk_gfx.h"
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
	string mesh_path = mainPath("test/test.model");
	string command = format("%s %s %s", mainPath("tools/model_convert").c_str(),
							mainPath("data/test.blend").c_str(), mesh_path.c_str());
	execCommand(command);
	XMLDocument doc;
	Loader(mesh_path) >> doc;
	PModel model = PModel(Model::loadFromXML(doc.child()));
	remove(mesh_path.c_str());

	int cube_id = model->findNodeId("Cube");
	int plane_id = model->findNodeId("Plane");
	int cone_id = model->findNodeId("Cone");

	ASSERT(cube_id != -1 && plane_id != -1 && cone_id != -1);
	const auto &nodes = model->nodes();
	ASSERT(nodes[plane_id]->parent()->id() == cube_id);

	auto pose = model->globalPose(model->defaultPose());
	vector<AffineTrans> transforms(begin(pose->transforms()), end(pose->transforms()));

	assertCloseEnough(transforms[cube_id].translation, float3(10, 0, 0));
	assertCloseEnough(transforms[plane_id].translation, float3(0, 0, -5));
	assertCloseEnough(transforms[cone_id].translation, float3(0, -3, 0));
}
