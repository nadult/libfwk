/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include <assimp/IOSystem.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

aiScene::~aiScene() {
	// TODO: add proper destructor
}

aiMaterial::aiMaterial() { memset(this, 0, sizeof(*this)); }

namespace fwk {

AssimpExporter::AssimpExporter() {
	for(size_t n = 0; n < aiGetExportFormatCount(); n++) {
		auto *desc = aiGetExportFormatDescription(n);
		m_formats.emplace_back(make_pair(string(desc->id), string(desc->fileExtension)));
	}
}

AssimpExporter::~AssimpExporter() = default;

uint AssimpExporter::defaultFlags() const { return 0; }

void AssimpExporter::saveScene(const aiScene *scene, const string &format_id, uint flags,
							   Stream &stream) {
	const aiExportDataBlob *blob = aiExportSceneToBlob(scene, format_id.c_str(), flags);
	try {
		stream.saveData(blob->data, blob->size);
	} catch(...) {
		aiReleaseExportBlob(blob);
		throw;
	}
	aiReleaseExportBlob(blob);
}

void AssimpExporter::saveScene(const aiScene *scene, const string &format_id, uint flags,
							   const string &file_name) {
	aiExportScene(scene, format_id.c_str(), file_name.c_str(), flags);
}

string AssimpExporter::findFormat(const string &ext) const {
	for(auto pair : m_formats)
		if(pair.second == ext)
			return pair.first;
	return "";
}
}
