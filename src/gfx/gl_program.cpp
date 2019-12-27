// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_program.h"

#include "fwk/format.h"
#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx/opengl.h"
#include "fwk/math/hash.h"
#include "fwk/math/matrix4.h"
#include "fwk/pod_vector.h"

namespace fwk {

namespace {

	void (*glProgramUniform1fv_)(GLuint, GLint, GLsizei, const GLfloat *);
	void (*glProgramUniform2fv_)(GLuint, GLint, GLsizei, const GLfloat *);
	void (*glProgramUniform3fv_)(GLuint, GLint, GLsizei, const GLfloat *);
	void (*glProgramUniform4fv_)(GLuint, GLint, GLsizei, const GLfloat *);
	void (*glProgramUniform1iv_)(GLuint, GLint, GLsizei, const GLint *);
	void (*glProgramUniform2iv_)(GLuint, GLint, GLsizei, const GLint *);
	void (*glProgramUniform3iv_)(GLuint, GLint, GLsizei, const GLint *);
	void (*glProgramUniform4iv_)(GLuint, GLint, GLsizei, const GLint *);
	void (*glProgramUniform1uiv_)(GLuint, GLint, GLsizei, const GLuint *);
	void (*glProgramUniform2uiv_)(GLuint, GLint, GLsizei, const GLuint *);
	void (*glProgramUniform3uiv_)(GLuint, GLint, GLsizei, const GLuint *);
	void (*glProgramUniform4uiv_)(GLuint, GLint, GLsizei, const GLuint *);

	void (*glProgramUniformMatrix2fv_)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
	void (*glProgramUniformMatrix3fv_)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);
	void (*glProgramUniformMatrix4fv_)(GLuint, GLint, GLsizei, GLboolean, const GLfloat *);

	void assignSSOFuncs() {
#ifndef FWK_TARGET_HTML
		glProgramUniform1fv_ = glProgramUniform1fv;
		glProgramUniform2fv_ = glProgramUniform2fv;
		glProgramUniform3fv_ = glProgramUniform3fv;
		glProgramUniform4fv_ = glProgramUniform4fv;

		glProgramUniform1iv_ = glProgramUniform1iv;
		glProgramUniform2iv_ = glProgramUniform2iv;
		glProgramUniform3iv_ = glProgramUniform3iv;
		glProgramUniform4iv_ = glProgramUniform4iv;

		glProgramUniform1uiv_ = glProgramUniform1uiv;
		glProgramUniform2uiv_ = glProgramUniform2uiv;
		glProgramUniform3uiv_ = glProgramUniform3uiv;
		glProgramUniform4uiv_ = glProgramUniform4uiv;

		glProgramUniformMatrix2fv_ = glProgramUniformMatrix2fv;
		glProgramUniformMatrix3fv_ = glProgramUniformMatrix3fv;
		glProgramUniformMatrix4fv_ = glProgramUniformMatrix4fv;
#endif
	}
}

void initializeGlProgramFuncs() {
	if(gl_info->features & GlFeature::separate_shader_objects) {
		assignSSOFuncs();
	} else {
#define ASSIGN(suffix, type)                                                                       \
	glProgramUniform##suffix##_ = [](GLuint prog, GLint location, GLsizei count,                   \
									 const type *data) {                                           \
		glUseProgram(prog);                                                                        \
		glUniform##suffix(location, count, data);                                                  \
	}

		ASSIGN(1fv, GLfloat);
		ASSIGN(2fv, GLfloat);
		ASSIGN(3fv, GLfloat);
		ASSIGN(4fv, GLfloat);

		ASSIGN(1iv, GLint);
		ASSIGN(2iv, GLint);
		ASSIGN(3iv, GLint);
		ASSIGN(4iv, GLint);

		ASSIGN(1uiv, GLuint);
		ASSIGN(2uiv, GLuint);
		ASSIGN(3uiv, GLuint);
		ASSIGN(4uiv, GLuint);
#undef ASSIGN

#define ASSIGN_MAT(size)                                                                           \
	glProgramUniformMatrix##size##fv_ = [](GLuint prog, GLint location, GLsizei count,             \
										   GLboolean trans, const GLfloat *data) {                 \
		glUseProgram(prog);                                                                        \
		glUniformMatrix##size##fv(location, count, trans, data);                                   \
	}
		ASSIGN_MAT(2);
		ASSIGN_MAT(3);
		ASSIGN_MAT(4);
#undef ASSIGN_MAT
	}
}

GL_CLASS_IMPL(GlProgram)

Ex<PProgram> GlProgram::make(PShader compute) {
	PProgram ref(storage.make());
	DASSERT(compute && compute->type() == ShaderType::compute);

	auto ret = ref->set({compute}, {});
	return ret ? ref : Ex<PProgram>(ret.error());
}

Ex<PProgram> GlProgram::make(PShader vertex, PShader fragment, CSpan<string> location_names) {
	PProgram ref(storage.make());
	DASSERT(vertex && vertex->type() == ShaderType::vertex);
	DASSERT(fragment && fragment->type() == ShaderType::fragment);

	auto ret = ref->set({vertex, fragment}, location_names);
	return ret ? ref : Ex<PProgram>(ret.error());
}

Ex<PProgram> GlProgram::make(PShader vertex, PShader geom, PShader fragment,
							 CSpan<string> location_names) {
	PProgram ref(storage.make());
	DASSERT(vertex && vertex->type() == ShaderType::vertex);
	DASSERT(geom && geom->type() == ShaderType::geometry);
	DASSERT(fragment && fragment->type() == ShaderType::fragment);

	auto ret = ref->set({vertex, geom, fragment}, location_names);
	return ret ? ref : Ex<PProgram>(ret.error());
}

Ex<PProgram> GlProgram::load(ZStr vsh_file_name, ZStr fsh_file_name, vector<string> sources,
							 CSpan<string> location_names) {
	auto vsh = EX_PASS(GlShader::load(ShaderType::vertex, vsh_file_name, sources));
	auto fsh = EX_PASS(GlShader::load(ShaderType::fragment, fsh_file_name, sources));
	return make(vsh, fsh, location_names);
}

Ex<void> GlProgram::set(CSpan<PShader> shaders, CSpan<string> loc_names) {
	m_hash = fwk::hash<u64>(loc_names);

	for(auto &shader : shaders) {
		glAttachShader(id(), shader.id());
		m_hash = combineHash(m_hash, shader->hash());
		if(shader->name() && m_name.empty())
			m_name = shader->name();
	}
	for(int l = 0; l < loc_names.size(); l++)
		glBindAttribLocation(id(), l, loc_names[l].c_str());

	glLinkProgram(id());
	auto err = glGetError();

	GLint param;
	glGetProgramiv(id(), GL_LINK_STATUS, &param);
	if(param == GL_FALSE) {
		glGetProgramiv(id(), GL_INFO_LOG_LENGTH, &param);
		PodVector<char> buffer(param);
		glGetProgramInfoLog(id(), buffer.size(), 0, buffer.data());
		buffer[buffer.size() - 1] = 0;

		return ERROR("Error while linking program:\n%", Str(buffer.data()));
	}

	for(auto &shader : shaders)
		glDetachShader(id(), shader.id());
	loadUniformInfo();
	return {};
}

string GlProgram::getInfo() const {
	char buf[4096];
	glGetProgramInfoLog(id(), 4096, 0, buf);
	return string(buf);
}

vector<char> GlProgram::getBinary() const {
	int size = 0;
	glGetProgramiv(id(), GL_PROGRAM_BINARY_LENGTH, &size);
	vector<char> out(size);
	GLenum format;
	glGetProgramBinary(id(), size, nullptr, &format, out.data());
	return out;
}

static const EnumMap<ProgramBindingType, int> binding_type_map = {{
	GL_SHADER_STORAGE_BLOCK,
	GL_UNIFORM_BLOCK,
	GL_ATOMIC_COUNTER_BUFFER,
	GL_TRANSFORM_FEEDBACK_BUFFER,
}};

vector<Pair<string, int>> GlProgram::getBindings(ProgramBindingType type) const {
	int num = 0, max_name = 0;
	auto type_id = binding_type_map[type];

	glGetProgramInterfaceiv(id(), type_id, GL_ACTIVE_RESOURCES, &num);
	glGetProgramInterfaceiv(id(), type_id, GL_MAX_NAME_LENGTH, &max_name);

	vector<Pair<string, int>> out;
	out.reserve(num);

	vector<char> buffer(max_name + 1);
	for(int n = 0; n < num; n++) {
		GLenum props[1] = {GL_BUFFER_BINDING};
		int values[1] = {0};
		glGetProgramResourceiv(id(), GL_SHADER_STORAGE_BLOCK, n, 1, props, 1, nullptr, values);

		int length = 0;
		glGetProgramResourceName(id(), GL_SHADER_STORAGE_BLOCK, n, buffer.size(), &length,
								 buffer.data());
		out.emplace_back(string(buffer.data(), (size_t)length), values[0]);
	}

	return out;
}

void GlProgram::UniformInfo::operator>>(TextFormatter &fmt) const {
	fmt(fmt.isStructured() ? "(%, %, %, %)" : "% % % %", name, gl_type, size, location);
}

Maybe<int> GlProgram::findUniform(Str str) const {
	for(auto &uniform : m_uniforms)
		if(str == uniform.name)
			return spanMemberIndex(m_uniforms, uniform);
	return none;
}

void GlProgram::loadUniformInfo() {
	if(m_uniforms)
		return;

	int count = 0, max_len = 0;
	glGetProgramiv(id(), GL_ACTIVE_UNIFORMS, &count);
	glGetProgramiv(id(), GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_len);

	m_uniforms.reserve(count);
	vector<char> data(max_len);

	for(int n = 0; n < count; n++) {
		int length = 0, size = 0;
		GLenum type;
		glGetActiveUniform(id(), n, data.size(), &length, &size, &type, data.data());
		int location = glGetUniformLocation(id(), data.data());
		m_uniforms.emplace_back(string(data.data(), length), (int)type, size, location);
	}
}

void GlProgram::use() { glUseProgram(id()); }
void GlProgram::unbind() { glUseProgram(0); }

auto GlProgram::operator[](Str name) -> UniformSetter {
	int loc = location(name);
	if((gl_debug_flags & GlDebug::not_active_uniforms) && loc == -1) {
		TextFormatter key, msg;
		key("% %", m_hash, name);
		msg("Program %: Uniform % not active", m_name, name);
		log(msg.text(), key.text());
	}
	return {id(), loc};
}

void GlProgram::UniformSetter::operator=(CSpan<float> range) {
	glProgramUniform1fv_(program_id, location, range.size(), range.data());
}

void GlProgram::UniformSetter::operator=(CSpan<float2> range) {
	glProgramUniform2fv_(program_id, location, range.size(), &range.data()->x);
}

void GlProgram::UniformSetter::operator=(CSpan<float3> range) {
	glProgramUniform3fv_(program_id, location, range.size(), &range.data()->x);
}

void GlProgram::UniformSetter::operator=(CSpan<float4> range) {
	glProgramUniform4fv_(program_id, location, range.size(), &range.data()->x);
}

void GlProgram::UniformSetter::operator=(CSpan<int> range) {
	glProgramUniform1iv_(program_id, location, range.size(), range.data());
}

void GlProgram::UniformSetter::operator=(CSpan<int2> range) {
	glProgramUniform2iv_(program_id, location, range.size(), &range.data()->x);
}

void GlProgram::UniformSetter::operator=(CSpan<int3> range) {
	glProgramUniform3iv_(program_id, location, range.size(), &range.data()->x);
}

void GlProgram::UniformSetter::operator=(CSpan<int4> range) {
	glProgramUniform4iv_(program_id, location, range.size(), &range.data()->x);
}

void GlProgram::UniformSetter::operator=(CSpan<uint> range) {
	glProgramUniform1uiv_(program_id, location, range.size(), range.data());
}

void GlProgram::UniformSetter::operator=(CSpan<Matrix4> range) {
	glProgramUniformMatrix4fv_(program_id, location, range.size(), false,
							   (const float *)range.data());
}

void GlProgram::UniformSetter::operator=(float value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(int value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(uint value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const int2 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const int3 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const int4 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const float2 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const float3 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const float4 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const Matrix4 &value) { *this = cspan(&value, 1); }
}
