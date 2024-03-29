// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/converter.h"

#include "fwk/enum_map.h"
#include "fwk/gfx/model.h"
#include "fwk/io/file_stream.h"
#include "fwk/io/file_system.h"
#include "fwk/io/xml.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/on_fail.h"

namespace fwk {

#define CVT_PRINT(...)                                                                             \
	{                                                                                              \
		if(m_settings.print_output)                                                                \
			print(__VA_ARGS__);                                                                    \
	}

using FileType = ModelFileType;

Converter::Converter(Settings settings) : m_settings(settings) {
	ASSERT(!m_settings.export_script_path.empty());
}

const EnumMap<FileType, string> Converter::s_extensions = {{
	{FileType::fwk_model, ".model"},
	{FileType::blender, ".blend"},
}};

Maybe<FileType> Converter::classify(const string &name) {
	string iname = toLower(name);

	for(auto type : all<FileType>) {
		auto pos = iname.rfind(s_extensions[type]);
		if(pos != string::npos && pos + s_extensions[type].size() == iname.size())
			return type;
	}

	return none;
}

Maybe<BlenderVersion> Converter::checkBlenderVersion(Str command) {
	if(auto result = execCommand(format("% --version", command))) {
		TextParser parser(result->first);
		string temp;
		double value;
		parser >> temp >> value;
		clearExceptions();
		if(temp == "Blender") {
			if(value >= 2.75 && value < 2.79)
				return BlenderVersion::ver_27x;
			if(value >= 2.80)
				return BlenderVersion::ver_28x;
		}
	}
	return none;
}

string Converter::exportScriptName(BlenderVersion ver) {
	return ver == BlenderVersion::ver_27x ? "export_fwk_model_279.py" : "export_fwk_model_280.py";
}

Ex<Converter::BlenderInfo> Converter::locateBlender() {
#if defined(FWK_PLATFORM_MINGW)
	vector<string> pfiles = {"C:/program files/", "C:/program files (x86)/"};
	for(auto pf : pfiles) {
		for(auto dir : fwk::findFiles(pf, FindFileOpt::directory)) {
			string folder_name = toLower(dir.path.fileName());
			if(folder_name.find("blender") != string::npos)
				if(auto files = fwk::findFiles(dir.path, "blender.exe")) {
					auto path = string(dir.path) + files[0] + "blender.exe";
					if(auto ver = checkBlenderVersion(path))
						return BlenderInfo{path, *ver};
				}
		}
	}
#else
	for(auto cmd : {"blender"})
		if(auto ver = checkBlenderVersion(cmd))
			return BlenderInfo{cmd, *ver};
#endif
	return ERROR("Cannot find blender with correct version (2.75+ or 2.80+)");
}

Ex<string> Converter::exportFromBlender(const string &file_name, string &target_file_name) {
	EXPECT(m_settings.blender_path);

	if(target_file_name.empty())
		target_file_name = file_name + ".model";
	string temp_script_name = file_name + ".py";

	auto script = EX_PASS(loadFileString(m_settings.export_script_path));
	string args = "\"" + target_file_name + "\"";
	script += "write(" + args + ")\n";

	saveFile(temp_script_name, cspan(script)).check();
	auto result = execCommand(format("\"%\" % --background --python % 2>&1",
									 *m_settings.blender_path, file_name, temp_script_name));

	::remove(temp_script_name.c_str());

	if(!result || result->second != 0) {
		auto err = result ? Error(result->first) : result.error();
		return err + format("Error while exporting from blender (file: %)", file_name);
	}

	if(m_settings.blender_output)
		CVT_PRINT("%\n", result->first);

	return result->first;
}

Ex<Pair<Model, string>> Converter::loadModel(FileType file_type, ZStr file_name) {
	if(file_type == FileType::fwk_model) {
		auto doc = std::move(XmlDocument::load(file_name).get());
		XmlOnFailGuard xml_guard(doc);
		XmlNode child = doc.child();
		if(!child)
			return ERROR("empty XML document");
		auto model = EX_PASS(Model::load(child));
		return pair{model, string(child.name())};
	} else {
		DASSERT(file_type == FileType::blender);
		string temp_file_name;
		auto blender_result = EX_PASS(exportFromBlender(file_name, temp_file_name));

		auto result = loadModel(FileType::fwk_model, temp_file_name);
		::remove(temp_file_name.c_str());

		if(result)
			return *result;
		return result.error() + ErrorChunk{blender_result};
	}
}

bool Converter::saveModel(const Model &model, const string &node_name, FileType file_type,
						  ZStr file_name) {
	if(file_type == FileType::fwk_model) {
		XmlDocument doc;
		XmlNode node = doc.addChild(doc.own(node_name));
		model.save(node);
		if(auto result = doc.save(file_name); !result) {
			CVT_PRINT("Error while saving: %\n", result.error());
			return false;
		}
	} else {
		CVT_PRINT("Unsupported file type for saving: %", file_type);
		return false;
	}
	return true;
}

bool Converter::operator()(const string &from, const string &to) {
	auto from_type = classify(from);
	auto to_type = classify(to);

	if(!from_type) {
		CVT_PRINT("Unsupported file type: '%'", from);
		return false;
	}
	if(!to_type) {
		CVT_PRINT("Unsupported file type: '%'", to);
		return false;
	}

	bool any_transforms = false;

	if(from_type == FileType::blender && to_type == FileType::fwk_model && !any_transforms &&
	   m_settings.just_export) {
		string target_name = to;
		return !!exportFromBlender(from, target_name);
	}

	CVT_PRINT("Loading: % (format: %)\n", from, from_type);
	auto pair = loadModel(*from_type, from);
	if(!pair) {
		CVT_PRINT("Error while loading\n%", pair.error());
		return false;
	}
	CVT_PRINT(" Nodes: %  Anims: %\n", pair->first.nodes().size(), pair->first.anims().size());
	CVT_PRINT(" Saving: % (node: %)\n\n", to, pair->second);

	return saveModel(pair->first, pair->second, *to_type, to);
}
}
