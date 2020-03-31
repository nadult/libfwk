// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/gfx_base.h"

namespace fwk {

DEFINE_ENUM(ModelFileType, fwk_model, blender);
DEFINE_ENUM(BlenderVersion, ver_27x, ver_28x)

// TODO: think about a better way to gather & report errors

class Converter {
  public:
	using FileType = ModelFileType;

	struct BlenderInfo {
		string path;
		BlenderVersion ver;
	};

	struct Settings {
		string export_script_path;
		Maybe<string> blender_path;
		bool just_export = false;
		bool blender_output = false;
		bool print_output = false;
	};

	Converter(Settings);

	bool operator()(const string &from, const string &to);

	static Maybe<BlenderVersion> checkBlenderVersion(Str command) NOEXCEPT;
	static string exportScriptName(BlenderVersion);
	static Ex<BlenderInfo> locateBlender();
	static Maybe<FileType> classify(const string &name);

	Ex<string> exportFromBlender(const string &file_name, string &target_file_name);
	Ex<Pair<Model, string>> loadModel(FileType file_type, ZStr file_name);
	bool saveModel(const Model &, const string &node_name, FileType file_type, ZStr file_name);

  private:
	Settings m_settings;
	static const EnumMap<FileType, string> s_extensions;
};
}
