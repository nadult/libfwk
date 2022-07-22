// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/io/file_system.h"
#include "fwk/tag_id.h"
#include "fwk/vector.h"
#include "fwk/vulkan_base.h"

namespace fwk {

struct ShaderCompilerOptions {
	VulkanVersion version;
	vector<FilePath> source_dirs;
	Maybe<FilePath> spirv_cache_dir;
	Maybe<int> glsl_version = 450;
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
	using Options = ShaderCompilerOptions;

	ShaderCompiler(Options);
	FWK_MOVABLE_CLASS(ShaderCompiler);

	struct CompilationResult {
		CompilationResult() {}
		CompilationResult(const Error &error) : messages(toString(error)) {}

		vector<char> bytecode;
		string messages;
	};

	Maybe<FilePath> filePath(ZStr file_name) const;
	CompilationResult compileCode(VShaderStage, ZStr source_code, ZStr file_name = "input.glsl",
								  CSpan<Pair<string>> macros = {}) const;
	CompilationResult compileFile(VShaderStage, ZStr file_name,
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
	vector<char> getSpirv(ShaderDefId def_id);
	vector<char> getSpirv(ZStr def_name);
	vector<ShaderDefId> updateList() const;

	vector<Pair<ShaderDefId, string>> getMessages();

  private:
	struct Impl;
	Dynamic<Impl> m_impl;
};

}