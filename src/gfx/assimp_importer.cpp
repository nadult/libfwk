/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include <assimp/Importer.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/postprocess.h>

namespace fwk {

class AssimpImporter::Impl : public Assimp::Importer {};

AssimpImporter::AssimpImporter() : m_impl(make_unique<Impl>()) {}
AssimpImporter::~AssimpImporter() = default;

const aiScene &AssimpImporter::loadScene(Stream &stream, uint flags, const char *extension_hint) {
	PodArray<char> data(stream.size() - stream.pos());
	stream.loadData(data.data(), data.size());

	if(!extension_hint)
		if(const char *last_dot = strrchr(stream.name(), '.'))
			extension_hint = last_dot + 1;

	const auto *scene = m_impl->ReadFileFromMemory(data.data(), data.size(), flags, extension_hint);
	if(!scene)
		THROW("Error while loading assimp::scene from file: %s\n%s", stream.name(),
			  m_impl->GetErrorString());
	return *scene;
}

const aiScene &AssimpImporter::loadScene(const char *data, int data_size, uint flags,
										 const char *extension_hint) {
	const auto *scene = m_impl->ReadFileFromMemory(data, data_size, flags, extension_hint);
	if(!scene)
		THROW("Error while loading assimp::scene from data:\n%s", m_impl->GetErrorString());
	return *scene;
}

const aiScene &AssimpImporter::loadScene(const string &file_name, uint flags) {
	const auto *scene = m_impl->ReadFile(file_name.c_str(), flags);
	if(!scene)
		THROW("Error while loading assimp::scene from file: %s\n%s", file_name.c_str(),
			  m_impl->GetErrorString());
	return *scene;
}

void AssimpImporter::freeScene() { m_impl->FreeScene(); }

uint AssimpImporter::defaultFlags() {
	return aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType |
		   aiProcess_ValidateDataStructure;
}
}
