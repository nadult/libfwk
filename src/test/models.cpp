/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "testing.h"
#include "fwk_gfx.h"

string mainPath(string file_name) {
	FilePath exec(executablePath());
	return exec.parent().parent() / file_name;
}

void testMain() {
	string mesh_path = mainPath("test/test.model");
	string command = format("%s %s %s", mainPath("tools/model_convert").c_str(),
							mainPath("data/test.blend").c_str(), mesh_path.c_str());
	execCommand(command);
	XMLDocument doc;
	Loader(mesh_path) >> doc;
	PModel model = make_shared<Model>(doc.child());
	remove(mesh_path.c_str());

	int cube_id = model->findNode("Cube");
	int plane_id = model->findNode("Plane");
	int cone_id = model->findNode("Cone");

	ASSERT(cube_id != -1 && plane_id != -1 && cone_id != -1);
	const auto &nodes = model->nodes();
	ASSERT(nodes[plane_id].parent_id == cube_id);

	auto pose = model->finalPose(model->defaultPose());
	vector<AffineTrans> transforms(begin(pose), end(pose));

	assertCloseEnough(transforms[cube_id].translation, float3(10, 0, 0));
	assertCloseEnough(transforms[plane_id].translation, float3(0, 0, -5));
	assertCloseEnough(transforms[cone_id].translation, float3(0, -3, 0));
}
