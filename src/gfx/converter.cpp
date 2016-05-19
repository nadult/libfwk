#include "fwk_mesh.h"

namespace fwk {

#define CVT_PRINT(...)                                                                             \
	{                                                                                              \
		if(m_settings.print_output)                                                                \
			printf(__VA_ARGS__);                                                                   \
	}

using FileType = ModelFileType;

Converter::Converter(Settings settings) : m_settings(settings) {
	if(m_settings.blender_path.empty())
		m_settings.blender_path = locateBlender();
	ASSERT(!m_settings.export_script_path.empty());
}

const EnumMap<FileType, string> Converter::s_extensions = {
	{FileType::fwk_model, ".model"}, {FileType::blender, ".blend"},
};

FileType Converter::classify(const string &name) {
	string iname = toLower(name);

	for(auto type : all<FileType>()) {
		auto pos = iname.rfind(s_extensions[type]);
		if(pos != string::npos && pos + s_extensions[type].size() == iname.size())
			return type;
	}

	THROW("Unsupported file type: %s\n", name.c_str());
	return FileType();
}

string Converter::locateBlender() {
#if defined(FWK_TARGET_MINGW) || defined(FWK_TARGET_MSVC)
	vector<string> pfiles = {"C:/program files/", "C:/program files (x86)/"};
	for(auto pf : pfiles) {
		for(auto dir : fwk::findFiles(pf, FindFiles::directory)) {
			string folder_name = toLower(dir.path.fileName());
			if(folder_name.find("blender") != string::npos) {
				auto files = fwk::findFiles(dir.path, "blender.exe");
				if(!files.empty())
					return string(dir.path) + files[0] + "blender.exe";
			}
		}
	}

	THROW("Cannot find blender");
	return "";
#else
	return "blender";
#endif
}

string Converter::exportFromBlender(const string &file_name, string &target_file_name) {
	if(target_file_name.empty())
		target_file_name = file_name + ".model";
	string temp_script_name = file_name + ".py";

	string script;
	{
		Loader loader(m_settings.export_script_path);
		script.resize(loader.size(), ' ');
		loader.loadData(&script[0], script.size());
		string args = "\"" + target_file_name + "\"";
		script += "write(" + args + ")\n";
	}

	Saver(temp_script_name).saveData(script.data(), script.size());
	auto result = execCommand(format("\"%s\" %s --background --python %s 2>&1",
									 m_settings.blender_path.c_str(), file_name.c_str(),
									 temp_script_name.c_str()));

	remove(temp_script_name.c_str());

	if(!result.second)
		THROW("Error while exporting from blender (file: %s):\n%s", file_name.c_str(),
			  result.first.c_str());

	if(m_settings.blender_output)
		CVT_PRINT("%s\n", result.first.c_str());

	return result.first;
}

pair<PModel, string> Converter::loadModel(FileType file_type, Stream &stream) {
	pair<PModel, string> out;

	if(file_type == FileType::fwk_model) {
		XMLDocument doc;
		stream >> doc;
		XMLNode child = doc.child();
		CHECK(child && "empty XML document");
		out = make_pair(immutable_ptr<Model>(Model::loadFromXML(child)), string(child.name()));
	} else {
		DASSERT(file_type == FileType::blender);
		CHECK(dynamic_cast<FileStream *>(&stream));
		string temp_file_name;
		auto blender_result = exportFromBlender(stream.name(), temp_file_name);

		try {
			Loader loader(temp_file_name);
			out = loadModel(FileType::fwk_model, loader);
			remove(temp_file_name.c_str());
		} catch(const Exception &ex) {
			THROW("%s\nBlender output:\n%s", ex.what(), blender_result.c_str());
		}
	}

	return out;
}

void Converter::saveModel(PModel model, const string &node_name, FileType file_type,
						  Stream &stream) {
	if(file_type == FileType::fwk_model) {
		XMLDocument doc;
		XMLNode node = doc.addChild(doc.own(node_name));
		model->saveToXML(node);
		stream << doc;
	} else
		THROW("Unsupported file type for saving: %s", toString(file_type));
}

void Converter::operator()(const string &from, const string &to) {
	auto from_type = classify(from);
	auto to_type = classify(to);

	bool any_transforms = false;

	if(from_type == FileType::blender && to_type == FileType::fwk_model && !any_transforms &&
	   m_settings.just_export) {
		string target_name = to;
		exportFromBlender(from, target_name);
		return;
	}

	Loader loader(from);
	Saver saver(to);

	CVT_PRINT("Loading: %s (format: %s)\n", from.c_str(), toString(from_type));
	auto pair = loadModel(from_type, loader);
	CVT_PRINT(" Nodes: %d  Anims: %d\n", (int)pair.first->nodes().size(),
			  (int)pair.first->anims().size());
	CVT_PRINT(" Saving: %s (node: %s)\n\n", to.c_str(), pair.second.c_str());

	saveModel(pair.first, pair.second, to_type, saver);
}
}
