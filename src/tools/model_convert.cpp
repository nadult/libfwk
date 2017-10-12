// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_base.h"
#include "fwk_mesh.h"

using namespace fwk;

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

int main(int argc, char **argv) {
	vector<string> params;
	if(argc == 1) {
		printHelp(argv[0]);
		exit(0);
	}

	Converter::Settings settings;
	settings.export_script_path = dataPath("export_fwk_model.py");
	settings.print_output = true;

	for(int n = 1; n < argc; n++) {
		string arg(argv[n]);

		if(arg[0] == '-' && arg[1] == '-') {
			if(arg == "--blender-just-export")
				settings.just_export = true;
			else if(arg == "--blender-path" && n + 1 < argc)
				settings.blender_path = argv[++n];
			else if(arg == "--blender-output")
				settings.blender_output = true;
			else if(arg == "--help") {
				printHelp(argv[0]);
				exit(0);
			} else {
				printf("Unsupported parameter: %s\n", argv[n]);
				exit(1);
			}
		} else {
			params.emplace_back(arg);
		}
	}

	if(params.size() != 2) {
		printf("Wrong number of parameters (see help)\n");
		exit(1);
	}

	Converter converter(settings);
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
				converter(src_name, dst_name);
			}
		}
	} else {
		converter(params[0], params[1]);
	}

	return 0;
}
