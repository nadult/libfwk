/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk.h"

using namespace fwk;

struct BlenderParams {
	BlenderParams() : just_export(false), print_output(false) {}
	string objects_filter;
	bool just_export;
	bool print_output;
};

static BlenderParams s_blender_params;

string dataPath(string file_name) {
	FilePath exec(executablePath());
	return exec.parent().parent() / "data" / file_name;
}

void printHelp(const char *app_name) {
	printf("Synopsis:\n"
		   "  %s [flags] [params]\n\n"
		   "Flags:\n"
		   // "  --transform \"1 0 0 0  0 1 0 0  0 0 1 0  0 0 0 1\"\n"
		   "  --blender-objects-filter \"human.*\"\n"
		   "  --blender-just-export\n"
		   "  --blender-print-output\n"
		   "Params:\n"
		   "  param 1:          source model\n"
		   "  param 2:          target model\n\n"
		   "Supported input formats:\n"
		   "  .blend (blender has to be available in the command line)\n"
		   "  .model\n\n"
		   "Supported output formats:\n"
		   "  .model\n"
		   "Examples:\n"
		   "  %s file.dae file.model\n"
		   "  %s file.blend file.model\n\n"
		   "  %s *.dae *.model\n\n",
		   app_name, app_name, app_name, app_name);
}

DECLARE_ENUM(FileType, fwk, blender);
DEFINE_ENUM(FileType, "fwk model", "blender");

struct FileExt {
	string ext;
	FileType::Type type;
};

const FileExt extensions[] = {
	{".model", FileType::fwk}, {".blend", FileType::blender},
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

string exportFromBlender(const string &file_name, string &target_file_name) {
	if(target_file_name.empty())
		target_file_name = file_name + ".model";
	string temp_script_name = file_name + ".py";

	string script;
	{
		string script_path = dataPath("export_fwk_model.py");

		Loader loader(script_path);
		script.resize(loader.size(), ' ');
		loader.loadData(&script[0], script.size());
		string args = "\"" + target_file_name + "\"";
		if(!s_blender_params.objects_filter.empty())
			args += ", objects_filter=\"" + s_blender_params.objects_filter + "\"";
		script += "write(" + args + ")\n";
	}

	Saver(temp_script_name).saveData(script.data(), script.size());
	auto result = execCommand(format("blender %s --background --python %s 2>&1", file_name.c_str(),
									 temp_script_name.c_str()));

	remove(temp_script_name.c_str());

	if(!result.second)
		THROW("Error while exporting from blender (file: %s):\n%s", file_name.c_str(),
			  result.first.c_str());

	if(s_blender_params.print_output)
		printf("%s\n", result.first.c_str());

	return result.first;
}

pair<PModel, string> loadModel(FileType::Type file_type, Stream &stream) {
	pair<PModel, string> out;

	if(file_type == FileType::fwk) {
		XMLDocument doc;
		stream >> doc;
		XMLNode child = doc.child();
		ASSERT(child && "empty XML document");
		out = make_pair(immutable_ptr<Model>(Model::loadFromXML(child)), string(child.name()));
	} else {
		DASSERT(file_type == FileType::blender);
		ASSERT(dynamic_cast<FileStream *>(&stream));
		string temp_file_name;
		auto blender_result = exportFromBlender(stream.name(), temp_file_name);

		try {
			Loader loader(temp_file_name);
			out = loadModel(FileType::fwk, loader);
			remove(temp_file_name.c_str());
		} catch(const Exception &ex) {
			THROW("%s\nBlender output:\n%s", ex.what(), blender_result.c_str());
		}
	}

	return out;
}

void saveModel(PModel model, const string &node_name, FileType::Type file_type, Stream &stream) {
	if(file_type == FileType::fwk) {
		XMLDocument doc;
		XMLNode node = doc.addChild(doc.own(node_name));
		model->saveToXML(node);
		stream << doc;
	} else
		THROW("Unsupported file type for saving: %s", toString(file_type));
}

void convert(const string &from, const Matrix4 &transform, const string &to) {
	FileType::Type from_type = classify(from);
	FileType::Type to_type = classify(to);

	bool any_transforms = false;

	if(from_type == FileType::blender && to_type == FileType::fwk && !any_transforms &&
	   s_blender_params.just_export) {
		string target_name = to;
		exportFromBlender(from, target_name);
		return;
	}

	Loader loader(from);
	Saver saver(to);

	printf("Loading: %s (format: %s)\n", from.c_str(), toString(from_type));
	auto pair = loadModel(from_type, loader);
	printf(" Nodes: %d  Anims: %d\n", (int)pair.first->nodes().size(),
		   (int)pair.first->anims().size());
	printf(" Saving: %s (node: %s)\n\n", to.c_str(), pair.second.c_str());

	saveModel(pair.first, pair.second, to_type, saver);
}

int safe_main(int argc, char **argv) {
	vector<string> params;
	if(argc == 1) {
		printHelp(argv[0]);
		exit(0);
	}

	Matrix4 transform = Matrix4::identity();

	for(int n = 1; n < argc; n++) {
		string arg(argv[n]);

		if(arg[0] == '-' && arg[1] == '-') {
			if(arg == "--transform" && n + 1 < argc)
				transform = xml_conversions::fromString<Matrix4>(argv[++n]);
			else if(arg == "--blender-objects-filter" && n + 1 < argc)
				s_blender_params.objects_filter = argv[++n];
			else if(arg == "--blender-just-export")
				s_blender_params.just_export = true;
			else if(arg == "--blender-print-output")
				s_blender_params.print_output = true;
			else if(arg == "--help") {
				printHelp(argv[0]);
				exit(0);
			}
			else {
				printf("Unsupported parameter: %s\n", argv[n]);
				exit(1);
			}
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
				convert(src_name, transform, dst_name);
			}
		}
	} else { convert(params[0], transform, params[1]); }

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
