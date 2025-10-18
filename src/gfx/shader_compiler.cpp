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

// TODO: an option to load shaders only from a SPIRV file
// TODO: Keeping a hierarchy of dependencies, updating files based on that

namespace fwk {

ZStr getShaderDebugCode();

// TODO: making sure that shaderc_shared.dll is available
// TODO: merge shader reflection module here?

ShaderDefinition::ShaderDefinition(string name, VShaderStage stage, string source_file_name,
								   vector<Pair<string>> macros)
	: name(std::move(name)), source_file_name(std::move(source_file_name)),
	  macros(std::move(macros)), stage(stage) {
	if(this->source_file_name.empty())
		this->source_file_name = this->name;
}

struct ShaderDefinitionEx : public ShaderDefinition {
	ShaderDefinitionEx(ShaderDefinition def) : ShaderDefinition(std::move(def)) {}

	vector<FilePath> update_paths;
	double last_modification_time = -1.0;
};

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
	vector<FilePath> current_paths;

	HashMap<string, ShaderDefId> shader_def_map;
	SparseVector<ShaderDefinitionEx> shader_defs;
	vector<Pair<ShaderDefId, string>> messages;

	vector<Pair<string>> internal_files;
	bool generate_assmbly = false;
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

	if(requested_source[0] == '%') {
		bool found = false;
		for(auto &[name, content] : impl.internal_files) {
			if(name == ZStr(requested_source + 1)) {
				new_result->path = requested_source;
				new_result->result.content = content.c_str();
				new_result->result.content_length = content.size();
				new_result->result.source_name = new_result->path.c_str();
				new_result->result.source_name_length = new_result->path.size();
				found = true;
				break;
			}
		}
		if(!found) {
			impl.include_errors.emplace_back(
				FWK_ERROR("Cannot find internal file: '%'", requested_source));
		}
	}

	FilePath req_path(requested_source);
	auto cur_path = FilePath(requesting_source).parent();
	if(auto file_path = impl.findPath(req_path, cur_path)) {
		impl.current_paths.emplace_back(*file_path);
		if(auto content = loadFileString(*file_path)) {
			new_result->path = *file_path;
			new_result->content = std::move(*content);
			auto &result = new_result->result;
			result.content = new_result->content.c_str();
			result.content_length = new_result->content.size();
			result.source_name = new_result->path.c_str();
			result.source_name_length = new_result->path.size();
		} else {
			impl.include_errors.emplace_back(std::move(content.error()));
		}
	}

	return &new_result->result;
}

ShaderCompiler::ShaderCompiler(ShaderCompilerSetup setup) {
	m_impl.emplace();
	m_impl->compiler = shaderc_compiler_initialize();
	m_impl->generate_assmbly = setup.generate_assembly;
	auto *opts = m_impl->compile_options = shaderc_compile_options_initialize();
	if(setup.debug_info)
		shaderc_compile_options_set_generate_debug_info(opts);

	if(setup.source_dirs) {
		for(auto &dir : setup.source_dirs)
			DASSERT(dir.isAbsolute());

		m_impl->source_dirs = std::move(setup.source_dirs);
		shaderc_compile_options_set_include_callbacks(opts, Impl::resolveInclude,
													  Impl::releaseInclude, m_impl.get());
	}
	uint vk_version = (setup.vulkan_version.major << 22) | (setup.vulkan_version.minor << 12);
	shaderc_compile_options_set_target_env(opts, shaderc_target_env_vulkan, vk_version);

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

	m_impl->spirv_cache_dir = std::move(setup.spirv_cache_dir);
	setInternalFile("shader_debug", getShaderDebugCode());
}

ShaderCompiler::~ShaderCompiler() {
	if(m_impl) {
		if(m_impl->compile_options)
			shaderc_compile_options_release(m_impl->compile_options);
		if(m_impl->compiler)
			shaderc_compiler_release(m_impl->compiler);
	}
}

ShaderCompiler::ShaderCompiler(ShaderCompiler &&rhs) : m_impl(std::move(rhs.m_impl)) {}
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
	Cleanup cleanup([&]() {
		if(macros)
			shaderc_compile_options_release(options);
	});

	auto result = shaderc_compile_into_spv(m_impl->compiler, code.c_str(), code.size(),
										   type_map[stage], file_name.c_str(), "main", options);

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
		return FWK_ERROR("Compilation of % shader '%' failed:\n%", stage, file_name, out.messages);
	m_impl->include_results.clear();

	if(m_impl->generate_assmbly) {
		auto result =
			shaderc_compile_into_spv_assembly(m_impl->compiler, code.c_str(), code.size(),
											  type_map[stage], file_name.c_str(), "main", options);
		auto status = shaderc_result_get_compilation_status(result);
		if(status == shaderc_compilation_status_success) {
			auto length = shaderc_result_get_length(result);
			auto *bytes = shaderc_result_get_bytes(result);
			out.assembly.assign(bytes, bytes + length);
		}
		shaderc_result_release(result);
		m_impl->include_errors.clear();
		m_impl->include_results.clear();
	}

	return out;
}

Ex<CompilationResult> ShaderCompiler::compileFile(VShaderStage stage, ZStr file_name,
												  CSpan<Pair<string>> macros) const {
	if(auto file_path = filePath(file_name)) {
		auto content = EX_PASS(loadFileString(*file_path));
		m_impl->current_paths.emplace_back(*file_path);
		return compileCode(stage, content, *file_path, macros);
	}

	return FWK_ERROR("Cannot open shader file: '%'", file_name);
}

// ------------------ ShaderDefs manager --------------------------------------------

ShaderDefId ShaderCompiler::add(ShaderDefinition def) {
	if(find(def.name))
		FWK_FATAL("ShaderDefinition already exists: '%s'", def.name.c_str());
	auto id = ShaderDefId(m_impl->shader_defs.nextFreeIndex());

	m_impl->shader_def_map.emplace(def.name, id);
	auto update_path = filePath(def.source_file_name);
	int index = m_impl->shader_defs.emplace(std::move(def));
	if(update_path)
		m_impl->shader_defs[index].update_paths.emplace_back(std::move(*update_path));
	return id;
}

const ShaderDefinition &ShaderCompiler::operator[](ShaderDefId id) const {
	DASSERT(valid(id));
	return m_impl->shader_defs[id];
}

bool ShaderCompiler::valid(ShaderDefId id) const { return m_impl->shader_defs.valid(id); }
void ShaderCompiler::remove(ShaderDefId id) {
	DASSERT(valid(id));
	auto &def = m_impl->shader_defs[id];
	m_impl->shader_def_map.erase(m_impl->shader_def_map.find(def.name));
	m_impl->shader_defs.erase(id);
}

void ShaderCompiler::setInternalFile(ZStr name, ZStr code) {
	DASSERT(!anyOf(name, internal_file_prefix));
	for(auto &pair : m_impl->internal_files)
		if(pair.first == name) {
			pair.second = code;
			return;
		}
	m_impl->internal_files.emplace_back(name, code);
}

void ShaderCompiler::removeInternalFile(ZStr name) {
	for(auto &pair : m_impl->internal_files)
		if(pair.first == name) {
			m_impl->internal_files.erase(&pair);
			return;
		}
}

Maybe<ShaderDefId> ShaderCompiler::find(ZStr name) const {
	return m_impl->shader_def_map.maybeFind(name);
}

const ShaderDefinition &ShaderCompiler::operator[](ZStr name) const {
	auto id = find(name);
	if(!id)
		FWK_FATAL("ShaderDefinition not found: '%s'", name.c_str());
	return m_impl->shader_defs[*id];
}

double lastModificationTime(CSpan<FilePath> paths) {
	double last_mod_time = -1.0;
	for(auto path : paths)
		if(auto mod_time = lastModificationTime(path))
			last_mod_time = max(last_mod_time, *mod_time);
	return last_mod_time;
}

// TODO: better way to pass errors
Ex<CompilationResult> ShaderCompiler::getSpirv(ShaderDefId id) {
	DASSERT(valid(id));
	auto &def = m_impl->shader_defs[id];
	Maybe<FilePath> spirv_path, asm_path;

	double last_mod_time = lastModificationTime(def.update_paths);

	if(m_impl->spirv_cache_dir) {
		spirv_path = *m_impl->spirv_cache_dir / (def.name + ".spv");
		asm_path = *m_impl->spirv_cache_dir / (def.name + ".asm");
		if(spirv_path->isRegularFile()) {
			double spirv_time = lastModificationTime(*spirv_path).orElse(-1.0);
			if(spirv_time >= last_mod_time) {
				if(auto spirv_data = loadFile(*spirv_path)) {
					CompilationResult result;
					result.bytecode = std::move(*spirv_data);
					def.last_modification_time = spirv_time;
					return result;
				}
			}
		}
	}

	// TODO: use latest spirv if compilation failed

	m_impl->current_paths.clear();
	auto result = EX_PASS(compileFile(def.stage, def.source_file_name, def.macros));
	makeSortedUnique(m_impl->current_paths);
	if(m_impl->current_paths != def.update_paths) {
		def.update_paths = std::move(m_impl->current_paths);
		last_mod_time = lastModificationTime(def.update_paths);
	}
	def.last_modification_time = last_mod_time;

	if(result.bytecode && spirv_path) {
		mkdirRecursive(spirv_path->parent()).check();
		saveFile(*spirv_path, result.bytecode).check();
	}
	if(result.assembly && asm_path) {
		mkdirRecursive(asm_path->parent()).check();
		saveFile(*asm_path, result.assembly).check();
	}

	return result;
}

Ex<CompilationResult> ShaderCompiler::getSpirv(ZStr def_name) {
	auto id = find(def_name);
	if(!id)
		return FWK_ERROR("ShaderDefinition not found: '%'", def_name);
	return getSpirv(*id);
}

Ex<PVShaderModule> ShaderCompiler::createShaderModule(VulkanDevice &device, ShaderDefId def_id) {
	auto result = EX_PASS(getSpirv(def_id));
	return VulkanShaderModule::create(device, result.bytecode);
}

Ex<PVShaderModule> ShaderCompiler::createShaderModule(VulkanDevice &device, ZStr def_name) {
	auto result = EX_PASS(getSpirv(def_name));
	return VulkanShaderModule::create(device, result.bytecode);
}

vector<ShaderDefId> ShaderCompiler::updateList() const {
	vector<ShaderDefId> out;
	HashMap<FilePath, double> last_mod_times;

	for(auto &def : m_impl->shader_defs) {
		for(auto &path : def.update_paths) {
			auto &time = last_mod_times[path];
			if(time == 0)
				time = lastModificationTime(path).orElse(-1.0);
			if(time > def.last_modification_time) {
				out.emplace_back(m_impl->shader_defs.indexOf(def));
				break;
			}
		}
	}

	return out;
}
}