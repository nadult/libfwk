// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/animated_model.h"
#include "fwk/gfx/converter.h"
#include "fwk/gfx/mesh.h"
#include "fwk/gfx/model.h"
#include "fwk/gfx/pose.h"
#include "fwk/io/file_stream.h"
#include "fwk/io/file_system.h"
#include "fwk/math/cylinder.h"
#include "fwk/math/triangle.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/xml.h"
#include "testing.h"

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
#ifndef FWK_PLATFORM_LINUX
	printf("TODO: tests/models is only supported on linux\n");
	return;
#endif

	auto blender_path = Converter::locateBlender();
	blender_path.check();

	Converter::Settings settings;
	settings.export_script_path = mainPath("data/export_fwk_model.py");
	settings.blender_path = *blender_path;

	Converter cvt(settings);
	auto mesh_path = mainPath("tests/test.model");
	auto result = cvt(mainPath("data/test.blend"), mesh_path);
	ASSERT(result);

	auto doc = move(XmlDocument::load(mesh_path).get());
	auto model = Model::load(doc.child());
	ASSERT(model);

	remove(mesh_path.c_str());

	auto tmesh = AnimatedModel(*model, model->defaultPose()).toMesh();
	auto tmesh_soup = Mesh::makePolySoup(tmesh.tris());
	ASSERT(tmesh.triangleCount() == tmesh_soup.triangleCount());

	int cube_id = model->findNodeId("cube");
	int plane_id = model->findNodeId("plane");
	int cone_id = model->findNodeId("cone");

	ASSERT(cube_id != -1 && plane_id != -1 && cone_id != -1);
	const auto &nodes = model->nodes();
	ASSERT_EQ(nodes[plane_id].parent_id, cube_id);

	auto pose = model->globalPose(model->defaultPose());
	auto transforms = transform<AffineTrans>(pose.transforms());

	assertCloseEnough(transforms[cube_id].translation, float3(10, 0, 0));
	assertCloseEnough(transforms[plane_id].translation, float3(0, 0, -5));
	assertCloseEnough(transforms[cone_id].translation, float3(0, -3, 0));
}
