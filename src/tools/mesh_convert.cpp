/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk.h"

using namespace fwk;

void printHelp(const char *app_name) {
	printf("Synopsis:\n"
		   "  %s [flags] [params]\n\n"
		   "Flags:\n"
		   "  --mesh (default): treat data as meshes\n"
		   "  --skinned-mesh:   treat data as skinned meshes\n"
		   //		   "  --simple-mesh:    treat data as simple mesh\n\n"
		   "Params:\n"
		   "  param 1:          source mesh\n"
		   "  param 2:          target mesh\n\n"
		   "Examples:\n"
		   "  %s --skinned-mesh file.dae file.mesh\n"
		   "  %s file.mesh file.dae\n\n"
		   "  %s *.mesh *.dae\n\n",
		   app_name, app_name, app_name, app_name);
}

enum class FileType {
	xml_mesh,
	assimp_mesh,
};

enum class MeshType {
	mesh,
	//	simple_mesh,
	skinned_mesh,
};

FileType classify(const string &name) {
	auto pos = name.rfind(".mesh");
	if(pos != string::npos && pos + 5 == name.size())
		return FileType::xml_mesh;
	return FileType::assimp_mesh;
}

template <class TMesh> auto loadMesh(FileType file_type, Stream &stream) {
	if(file_type == FileType::xml_mesh) {
		XMLDocument doc;
		stream >> doc;
		XMLNode child = doc.child();
		ASSERT(child && "empty XML document");
		return make_pair(make_shared<TMesh>(child), string(child.name()));
	}
	DASSERT(file_type == FileType::assimp_mesh);
	{
		AssimpImporter importer;
		auto mesh = make_shared<TMesh>(importer.loadScene(stream, AssimpImporter::defaultFlags()));
		return make_pair(mesh, string("mesh"));
	}
}

template <class TMesh>
void saveMesh(shared_ptr<TMesh> mesh, const string &node_name, FileType file_type, Stream &stream) {
	if(file_type == FileType::xml_mesh) {
		XMLDocument doc;
		XMLNode node = doc.addChild(doc.own(node_name));
		mesh->saveToXML(node);
		stream << doc;
	} else if(file_type == FileType::assimp_mesh) { THROW("Write me"); } else
		THROW("Unsupported file type");
}

template <class TMesh> void convert(const string &from, const string &to) {
	FileType from_type = classify(from);
	FileType to_type = classify(to);

	Loader loader(from);
	Saver saver(to);

	printf("Loading: %s (format: %s)\n", from.c_str(),
		   from_type == FileType::xml_mesh ? "xml" : "assimp");
	auto pair = loadMesh<TMesh>(from_type, loader);
	printf(" Saving: %s (node: %s)\n\n", to.c_str(), pair.second.c_str());
	saveMesh(pair.first, pair.second, to_type, saver);
}

void convert(MeshType mesh_type, const string &from, const string &to) {
	if(mesh_type == MeshType::mesh)
		convert<Mesh>(from, to);
	//	else if(mesh_type == MeshType::simple_mesh)
	//		convert<SimpleMesh>(from, to);
	else if(mesh_type == MeshType::skinned_mesh)
		convert<SkinnedMesh>(from, to);
}

int main(int argc, char **argv) {
	vector<string> params;
	MeshType mesh_type = MeshType::mesh;
	if(argc == 1) {
		printHelp(argv[0]);
		exit(0);
	}

	for(int n = 1; n < argc; n++) {
		string arg(argv[n]);

		if(arg[0] == '-' && arg[1] == '-') {
			if(arg == "--help") {
				printHelp(argv[0]);
				exit(0);
			}
			if(arg == "--mesh")
				mesh_type = MeshType::mesh;
			//			if(arg == "--simple-mesh")
			//				mesh_type = MeshType::simple_mesh;
			if(arg == "--skinned-mesh")
				mesh_type = MeshType::skinned_mesh;
		} else { params.emplace_back(arg); }
	}

	if(params.size() != 2) {
		printf("Wrong number of parameters (see help)\n");
		exit(1);
	}

	if(params[0].find('*') != string::npos) {
		auto star_pos = params[0].find('*');
		string prefix = params[0].substr(0, star_pos);
		string suffix = params[0].substr(star_pos + 1);

		star_pos = params[1].find('*');
		ASSERT(star_pos != string::npos &&
			   "with multiple sources, you need multiple targets as well");
		string tprefix = params[1].substr(0, star_pos);
		string tsuffix = params[1].substr(star_pos + 1);

		FilePath src_folder(prefix);
		while(!src_folder.isDirectory())
			src_folder = src_folder.parent();
		auto files = findFiles(src_folder, FindFiles::regular_file | FindFiles::recursive);

		for(const auto &file : files) {
			string name = file.path;
			if(removePrefix(name, prefix) && removeSuffix(name, suffix)) {
				string src_name = file.path;
				string dst_name = tprefix + name + tsuffix;
				FilePath dst_folder = FilePath(dst_name).parent();
				if(!access(dst_folder))
					mkdirRecursive(dst_folder);
				convert(mesh_type, src_name, dst_name);
			}
		}
	} else { convert(mesh_type, params[0], params[1]); }

	return 0;
}
