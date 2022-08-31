// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/io/file_system.h"
#include "fwk/tag_id.h"
#include "fwk/vector.h"
#include "fwk/vulkan_base.h"

namespace fwk {

struct ShaderCompilerSetup {
	vector<FilePath> source_dirs;
	Maybe<FilePath> spirv_cache_dir;
	VulkanVersion vulkan_version = {1, 2, 0};
	Maybe<double> glsl_version = 4.5;
	Maybe<double> spirv_version = 1.3;
	bool debug_info = false;
};

struct ShaderDefinition {
	ShaderDefinition(string name, VShaderStage stage, string source_file_name = {},
					 vector<Pair<string>> macros = {});

	string name;
	string source_file_name;
	vector<Pair<string>> macros;
	VShaderStage stage;
};

using ShaderDefId = TagId<Tag::shader_def>;

class ShaderCompiler {
  public:
	ShaderCompiler(ShaderCompilerSetup = {});
	FWK_MOVABLE_CLASS(ShaderCompiler);

	struct CompilationResult {
		vector<char> bytecode;
		string messages;
	};

	Maybe<FilePath> filePath(ZStr file_name) const;
	Ex<CompilationResult> compileCode(VShaderStage, ZStr source_code, ZStr file_name = "input.glsl",
									  CSpan<Pair<string>> macros = {}) const;
	Ex<CompilationResult> compileFile(VShaderStage, ZStr file_name,
									  CSpan<Pair<string>> macros = {}) const;

	// ------------------ ShaderDefs manager --------------------------------------------

	// When adding def, you have to make sure that there are no name collisions
	ShaderDefId add(ShaderDefinition);
	const ShaderDefinition &operator[](ShaderDefId) const;
	bool valid(ShaderDefId) const;
	void remove(ShaderDefId);

	Maybe<ShaderDefId> find(ZStr shader_def_name) const;
	const ShaderDefinition &operator[](ZStr) const;

	// If source code failed to compile, latest valid spirv will be returned
	Ex<CompilationResult> getSpirv(ShaderDefId def_id);
	Ex<CompilationResult> getSpirv(ZStr def_name);
	Ex<PVShaderModule> createShaderModule(VDeviceRef, ZStr def_name);

	vector<ShaderDefId> updateList() const;

  private:
	struct Impl;
	Dynamic<Impl> m_impl;
};

}