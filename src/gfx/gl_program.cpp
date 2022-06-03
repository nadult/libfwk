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
#ifndef FWK_PLATFORM_HTML
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

PProgram GlProgram::link(CSpan<PShader> shaders, CSpan<string> location_names) {
	PProgram ref(storage.make());
	ref->m_hash = fwk::hash<u64>(location_names);
	auto id = ref->id();

	for(auto &shader : shaders) {
		ASSERT(shader->isCompiled());
		glAttachShader(id, shader.id());
		ref->m_hash = combineHash(ref->m_hash, shader->hash());
	}
	for(int l = 0; l < location_names.size(); l++)
		glBindAttribLocation(id, l, location_names[l].c_str());

	glLinkProgram(id);
	auto err = glGetError(); // TODO: shoud we ignore this?
	for(auto &shader : shaders)
		glDetachShader(id, shader.id());

	if(ref->isLinked())
		ref->loadUniformInfo();
	return ref;
}

Ex<PProgram> GlProgram::linkAndCheck(CSpan<PShader> shaders, CSpan<string> location_names) {
	auto result = link(shaders, location_names);
	if(!result->isLinked())
		return ERROR("Shader linking error:\n%", result->linkLog());
	return result;
}

bool GlProgram::isLinked() const {
	GLint status;
	glGetProgramiv(id(), GL_LINK_STATUS, &status);
	return status == GL_TRUE;
}

string GlProgram::linkLog() const {
	GLint param;
	auto id = this->id();
	glGetProgramiv(id, GL_INFO_LOG_LENGTH, &param);
	PodVector<char> buffer(param);
	glGetProgramInfoLog(id, buffer.size(), 0, buffer.data());
	buffer[buffer.size() - 1] = 0;
	return buffer.data();
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
		msg("GlProgram '%': Uniform % not active", m_label, name);
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

void GlProgram::UniformSetter::operator=(CSpan<Matrix3> range) {
	glProgramUniformMatrix3fv_(program_id, location, range.size(), false,
							   (const float *)range.data());
}

void GlProgram::UniformSetter::operator=(float value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(int value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(uint value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(bool value) { *this = int(value); }
void GlProgram::UniformSetter::operator=(const int2 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const int3 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const int4 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const float2 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const float3 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const float4 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const Matrix4 &value) { *this = cspan(&value, 1); }
void GlProgram::UniformSetter::operator=(const Matrix3 &value) { *this = cspan(&value, 1); }
}
