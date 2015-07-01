/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk.h"
#include <assimp/scene.h>

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
		   "Supported input formats:\n"
		   "  .blend (blender has to be available in the command line)\n"
		   "  .dae\n"
		   "  .mesh\n\n"
		   "Supported output formats:\n"
		   "  .mesh\n"
		   "  .dae (assimp exporter doesn't support animations and is kinda broken)\n\n"
		   "Examples:\n"
		   "  %s --skinned-mesh file.dae file.mesh\n"
		   "  %s file.blend file.mesh\n\n"
		   "  %s *.dae *.mesh\n\n",
		   app_name, app_name, app_name, app_name);
}

DECLARE_ENUM(FileType, fwk, blender, assimp);
DEFINE_ENUM(FileType, "fwk mesh", "blender", "assimp");

struct FileExt {
	string ext;
	FileType::Type type;
};

const FileExt extensions[] = {
	{".dae", FileType::assimp}, {".mesh", FileType::fwk}, {".blend", FileType::blender},
};

enum class MeshType {
	mesh,
	//	simple_mesh,
	skinned_mesh,
};

FileType::Type classify(const string &name) {
	string iname = name;
	std::transform(begin(iname), end(iname), begin(iname), tolower);

	for(auto &ext : extensions) {
		auto pos = iname.rfind(ext.ext);
		if(pos != string::npos && pos + ext.ext.size() == iname.size())
			return ext.type;
	}

	THROW("Unsupported file type: %s\n", name.c_str());
	return FileType::invalid;
}

template <class TMesh> auto loadMesh(FileType::Type file_type, Stream &stream) {
	if(file_type == FileType::fwk) {
		XMLDocument doc;
		stream >> doc;
		XMLNode child = doc.child();
		ASSERT(child && "empty XML document");
		return make_pair(make_shared<TMesh>(child), string(child.name()));
	}
	if(file_type == FileType::blender) {
		ASSERT(dynamic_cast<FileStream *>(&stream));
		string file_name = stream.name();
		string temp_script_name = file_name + ".py";
		string temp_file_name = file_name + ".dae";
		string script = format("import bpy\nbpy.ops.wm.collada_export(filepath=\"%s\")\n",
							   temp_file_name.c_str());

		Saver(temp_script_name).saveData(script.data(), script.size());

		execCommand(format("blender %s --background --python %s 2>/dev/null", file_name.c_str(),
						   temp_script_name.c_str()));

		remove(temp_script_name.c_str());
		Loader loader(temp_file_name);
		auto out = loadMesh<TMesh>(FileType::assimp, loader);
		remove(temp_file_name.c_str());
		return std::move(out);
	}
	DASSERT(file_type == FileType::assimp);
	{
		AssimpImporter importer;
		auto mesh = make_shared<TMesh>(importer.loadScene(stream, AssimpImporter::defaultFlags()));
		return make_pair(mesh, string("mesh"));
	}
}

template <class TMesh>
void saveMesh(shared_ptr<TMesh> mesh, const string &node_name, FileType::Type file_type,
			  Stream &stream) {
	if(file_type == FileType::fwk) {
		XMLDocument doc;
		XMLNode node = doc.addChild(doc.own(node_name));
		mesh->saveToXML(node);
		stream << doc;
	} else if(file_type == FileType::assimp) {
		AssimpExporter exporter;
		auto *scene = mesh->toAIScene();
		string extension = FilePath(stream.name()).fileExtension();
		string format_id = exporter.findFormat(extension);
		if(format_id.empty())
			THROW("Assimp doesn't support exporting to '*.%s' files", extension.c_str());
		printf("Keep in mind: assimp isn't so good when it comes to exporting data...\n");
		auto format = exporter.formats().front();
		exporter.saveScene(scene, format.first, 0, stream);
	} else
		THROW("Unsupported file type: %s", toString(file_type));
}

template <class TMesh> void convert(const string &from, const string &to) {
	FileType::Type from_type = classify(from);
	FileType::Type to_type = classify(to);

	Loader loader(from);
	Saver saver(to);

	printf("Loading: %s (format: %s)\n", from.c_str(), toString(from_type));
	auto pair = loadMesh<TMesh>(from_type, loader);
	printf(" Parts: %d  Nodes: %d  Anims: %d\n", (int)pair.first->meshes().size(),
		   (int)pair.first->nodes().size(), (int)pair.first->anims().size());
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

int safe_main(int argc, char **argv) {
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

int main(int argc, char **argv) {
	try {
		return safe_main(argc, argv);
	} catch(const Exception &ex) {
		printf("%s\n\nBacktrace:\n%s\n", ex.what(), cppFilterBacktrace(ex.backtrace()).c_str());
		return 1;
	}
}
