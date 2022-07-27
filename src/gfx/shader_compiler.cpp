// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/shader_compiler.h"

#include "fwk/enum_map.h"
#include "fwk/format.h"
#include "fwk/hash_map.h"
#include "fwk/io/file_system.h"
#include "fwk/sparse_vector.h"
#include "fwk/vulkan/vulkan_shader.h"
#include "shaderc/shaderc.h"

// Powinna byæ mo¿liwoœæ ³adowania shaderów tylko z plików spirv
// Czy wogóle potrzebujemy obs³ugê makr? mo¿e lepiej obs³ugiwaæ tylko sta³e?

// Makra móg³bym przerobiæ na include-owane pliki (tak jak shader debug)
// OK, ale w takim systemie jak nazywaæ pliki spriv? Jeœli mo¿na wygenerowaæ
// wiele opcji podaj¹c ró¿ne pliki wejœciowe?

// System powinien te¿ trackowaæ hierarchiê zale¿noœci, i jeœli którykolwiek
// z includowanych plików siê zmieni³, to propagowa³ te zmiany wy¿ej

// Jedna kwestia to trackowanie zmian w plikach Ÿród³owych

// Czy lepsze jest podawanie makr jako parametrów czy przez dodtkowy plik?
// funkcjonalnoœæ przez plik i tak trzeba bêdzie zrobiæ...

// Problem: shader debug mogê w³¹czaæ tylko dla niektórych shaderów, jak to w³¹czaæ / wy³¹czaæ?
// Mo¿e móg³bym trzymaæ dwie wersje tego samego shadera? raster_low.spv i raster_low_dbg.spv ?
// Jeœli wypuszczê wersjê tylko _spv, to muszê mieæ wygenerowane wszystkie warianty .spv które
// bêd¹ potrzebne (albo kontrolowaæ wiêkszoœæ zmian za pomoc¹ specjalizacji).

// Jak nazywaæ ró¿ne wersje?
// u¿ytkownik mia³by mo¿liwoœæ nazywania wersji w trybie spirv?
//
// makra mog¹ byæ, ale one nie bêd¹ wp³ywaæ na nazwê spv, to jest odpowiedzialnoœæ u¿ytkownika?
// Ale w takiej sytuacji, mo¿e powstaæ kolizja nazw ...
//
// To mo¿e zróbmy z makrami i z haszowaniem makr dla uzyskania suffiksu nazwy spv
// Dodatkowo u¿ytkownik mo¿e tak¹ nazwê zoverride-owaæ
//

namespace fwk {

// TODO: making sure that shaderc_shared.dll is available
// TODO: merge shader reflection module here?

ShaderDefinition::ShaderDefinition(string name, VShaderStage stage, string source_file_name,
								   vector<Pair<string>> macros)
	: name(move(name)), source_file_name(move(source_file_name)), macros(move(macros)),
	  stage(stage) {
	if(this->source_file_name.empty())
		this->source_file_name = this->name;
}

using CompilationResult = ShaderCompiler::CompilationResult;

struct ShaderCompiler::Impl {
	Maybe<FilePath> findPath(FilePath path) const;
	Maybe<FilePath> findPath(FilePath path, FilePath cur_path) const;

	static shaderc_include_result *resolveInclude(void *user_data, const char *requested_source,
												  int type, const char *requesting_source,
												  size_t include_depth);
	static void releaseInclude(void *, shaderc_include_result *) {
		// Includes will be released when compilation ends
	}

	shaderc_compiler_t compiler = nullptr;
	shaderc_compile_options_t compile_options = nullptr;
	struct IncludeResult {
		string path;
		string content;
		shaderc_include_result result{};
	};
	vector<Dynamic<IncludeResult>> include_results;
	vector<FilePath> source_dirs;
	Maybe<FilePath> spirv_cache_dir;
	vector<Error> include_errors;

	HashMap<string, ShaderDefId> shader_def_map;
	SparseVector<ShaderDefinition> shader_defs;
	vector<Pair<ShaderDefId, string>> messages;
};

Maybe<FilePath> ShaderCompiler::Impl::findPath(FilePath path) const {
	if(path.isAbsolute())
		return path;
	for(auto &dir : source_dirs) {
		auto sub_path = dir / path;
		if(sub_path.isRegularFile())
			return sub_path;
	}
	return none;
}

Maybe<FilePath> ShaderCompiler::Impl::findPath(FilePath path, FilePath cur_path) const {
	if(path.isAbsolute())
		return path;
	auto sub_path = cur_path / path;
	if(sub_path.isRegularFile())
		return sub_path;
	return findPath(path);
}

shaderc_include_result *ShaderCompiler::Impl::resolveInclude(void *user_data,
															 const char *requested_source, int type,
															 const char *requesting_source,
															 size_t include_depth) {
	auto &impl = *(Impl *)user_data;
	IncludeResult *new_result = new IncludeResult;
	impl.include_results.emplace_back(new_result);

	FilePath req_path(requested_source);
	auto cur_path = FilePath(requesting_source).parent();
	if(auto file_path = impl.findPath(req_path, cur_path)) {
		if(auto content = loadFileString(*file_path)) {
			new_result->path = *file_path;
			new_result->content = move(*content);
			auto &result = new_result->result;
			result.content = new_result->content.c_str();
			result.content_length = new_result->content.size();
			result.source_name = new_result->path.c_str();
			result.source_name_length = new_result->path.size();
		} else {
			impl.include_errors.emplace_back(move(content.error()));
		}
	}

	return &new_result->result;
}

ShaderCompiler::ShaderCompiler(ShaderCompilerSetup setup) {
	m_impl.emplace();
	m_impl->compiler = shaderc_compiler_initialize();
	auto *opts = m_impl->compile_options = shaderc_compile_options_initialize();
	if(setup.source_dirs) {
		for(auto &dir : setup.source_dirs)
			DASSERT(dir.isAbsolute());

		m_impl->source_dirs = move(setup.source_dirs);
		shaderc_compile_options_set_include_callbacks(opts, Impl::resolveInclude,
													  Impl::releaseInclude, m_impl.get());
	}
	uint vk_version = (setup.vulkan_version.major << 22) | (setup.vulkan_version.minor << 12);
	shaderc_compile_options_set_target_env(opts, shaderc_target_env_vulkan, vk_version);
	if(setup.glsl_version)
		shaderc_compile_options_set_forced_version_profile(opts, int(*setup.glsl_version * 100),
														   shaderc_profile_none);
	if(setup.spirv_version) {
		int version = int(*setup.spirv_version * 10);
		int major = version / 10;
		int minor = version % 10;
		DASSERT(major == 1);
		DASSERT(minor >= 0 && minor <= 6);
		uint version_id = 0x010000u | (minor << 8);
		shaderc_compile_options_set_target_spirv(opts, shaderc_spirv_version(version_id));
	}
	shaderc_compile_options_set_optimization_level(opts, shaderc_optimization_level_performance);

	m_impl->spirv_cache_dir = move(setup.spirv_cache_dir);
}

ShaderCompiler::~ShaderCompiler() {
	if(m_impl) {
		if(m_impl->compile_options)
			shaderc_compile_options_release(m_impl->compile_options);
		if(m_impl->compiler)
			shaderc_compiler_release(m_impl->compiler);
	}
}

ShaderCompiler::ShaderCompiler(ShaderCompiler &&rhs) : m_impl(move(rhs.m_impl)) {}
FWK_MOVE_ASSIGN_RECONSTRUCT(ShaderCompiler)

using Stage = VShaderStage;
const EnumMap<VShaderStage, shaderc_shader_kind> type_map = {
	{shaderc_vertex_shader, shaderc_tess_control_shader, shaderc_tess_evaluation_shader,
	 shaderc_geometry_shader, shaderc_fragment_shader, shaderc_compute_shader}};

Maybe<FilePath> ShaderCompiler::filePath(ZStr name) const { return m_impl->findPath(name); }

Ex<CompilationResult> ShaderCompiler::compileCode(VShaderStage stage, ZStr code, ZStr file_name,
												  CSpan<Pair<string>> macros) const {
	CompilationResult out;

	auto *options = m_impl->compile_options;
	if(macros) {
		options = shaderc_compile_options_clone(options);
		for(auto &[name, value] : macros)
			shaderc_compile_options_add_macro_definition(options, name.c_str(), name.size(),
														 value.c_str(), value.size());
	}
	auto result = shaderc_compile_into_spv(m_impl->compiler, code.c_str(), code.size(),
										   type_map[stage], file_name.c_str(), "main", options);
	if(macros)
		shaderc_compile_options_release(options);

	int num_warnings = shaderc_result_get_num_warnings(result);
	int num_errors = shaderc_result_get_num_errors(result);
	auto status = shaderc_result_get_compilation_status(result);

	if(status == shaderc_compilation_status_success) {
		auto length = shaderc_result_get_length(result);
		auto *bytes = shaderc_result_get_bytes(result);
		out.bytecode.assign(bytes, bytes + length);
	}
	out.messages = shaderc_result_get_error_message(result);
	shaderc_result_release(result);

	if(m_impl->include_errors)
		return Error::merge(m_impl->include_errors);
	if(!out.bytecode)
		return ERROR("Compilation failed:\n%", out.messages);

	m_impl->include_results.clear();
	m_impl->include_errors.clear();
	return out;
}

Ex<CompilationResult> ShaderCompiler::compileFile(VShaderStage stage, ZStr file_name,
												  CSpan<Pair<string>> macros) const {
	if(auto file_path = filePath(file_name)) {
		auto content = EX_PASS(loadFileString(*file_path));
		return compileCode(stage, content, *file_path, macros);
	}

	return ERROR("Cannot open shader file: '%'", file_name);
}

// ------------------ ShaderDefs manager --------------------------------------------

ShaderDefId ShaderCompiler::add(ShaderDefinition def) {
	if(find(def.name))
		FATAL("ShaderDefinition already exists: '%s'", def.name.c_str());
	auto id = ShaderDefId(m_impl->shader_defs.nextFreeIndex());
	m_impl->shader_def_map.emplace(def.name, id);
	m_impl->shader_defs.emplace(move(def));
	return id;
}

const ShaderDefinition &ShaderCompiler::operator[](ShaderDefId id) const {
	DASSERT(valid(id));
	return m_impl->shader_defs[id];
}

bool ShaderCompiler::valid(ShaderDefId id) const { return m_impl->shader_defs.valid(id); }
void ShaderCompiler::remove(ShaderDefId id) {
	DASSERT(valid(id));
	m_impl->shader_defs.erase(id);
}

Maybe<ShaderDefId> ShaderCompiler::find(ZStr name) const {
	return m_impl->shader_def_map.maybeFind(name);
}

const ShaderDefinition &ShaderCompiler::operator[](ZStr name) const {
	auto id = find(name);
	if(!id)
		FATAL("ShaderDefinition not found: '%s'", name.c_str());
	return m_impl->shader_defs[*id];
}

Ex<CompilationResult> ShaderCompiler::getSpirv(ShaderDefId id) {
	DASSERT(valid(id));
	auto &def = m_impl->shader_defs[id];
	FilePath spirv_path;

	if(m_impl->spirv_cache_dir) {
		spirv_path = *m_impl->spirv_cache_dir / (def.name + ".spv");
		if(spirv_path.isRegularFile()) {
			// TODO: handle error properly
			// TODO: for now loading disabled
			//return loadFile(spirv_path).get();
		}
	}

	auto result = EX_PASS(compileFile(def.stage, def.source_file_name, def.macros));
	if(result.bytecode && !spirv_path.empty()) {
		mkdirRecursive(spirv_path.parent()).check();
		saveFile(spirv_path, result.bytecode).check();
	}

	return result;
}

// TODO: better way to pass errors
Ex<CompilationResult> ShaderCompiler::getSpirv(ZStr def_name) {
	auto id = find(def_name);
	if(!id)
		return ERROR("ShaderDefinition not found: '%'", def_name);
	return getSpirv(*id);
}

Ex<PVShaderModule> ShaderCompiler::createShaderModule(VDeviceRef device, ZStr def_name) {
	auto result = EX_PASS(getSpirv(def_name));
	return VulkanShaderModule::create(device, result.bytecode);
}

vector<ShaderDefId> ShaderCompiler::updateList() const { return {}; }

}