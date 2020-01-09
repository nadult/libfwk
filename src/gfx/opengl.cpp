// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifdef FWK_GFX_OPENGL_H
#define FWK_GFX_OPENGL_H_ONLY_EXTENSIONS
#undef FWK_GFX_OPENGL_H
#endif

#define EXT_API

#include "fwk/gfx/opengl.h"

#include "fwk/enum_map.h"
#include "fwk/format.h"
#include "fwk/gfx/color.h"
#include "fwk/parse.h"
#include "fwk/str.h"
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace fwk {

void *winLoadFunction(const char *name);

static GlInfo s_info;
const GlInfo *const gl_info = &s_info;
GlDebugFlags gl_debug_flags = none;

static EnumMap<GlLimit, int> s_limit_map = {
	{GL_MAX_ELEMENTS_INDICES, GL_MAX_ELEMENTS_VERTICES, GL_MAX_UNIFORM_BLOCK_SIZE,
	 GL_MAX_TEXTURE_SIZE, GL_MAX_UNIFORM_LOCATIONS, GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS}};

string GlInfo::toString() const {
	TextFormatter out;
	out("Vendor: %\nRenderer: %\nProfile: %\nExtensions: %\nVersion: % (%)\nGLSL version: % (%)\n",
		vendor, renderer, profile, extensions, version, version_full, glsl_version,
		glsl_version_full);

	out("Limits:\n");
	for(auto limit : all<GlLimit>)
		out("  %: %\n", limit, limits[limit]);

	return out.text();
}

bool GlInfo::hasExtension(Str name) const {
	auto it = std::lower_bound(begin(extensions), end(extensions), name,
							   [](Str a, Str b) { return a < b; });

	return it != end(extensions) && Str(*it) == name;
}

static void loadExtensions();

static float parseOpenglVersion(const char *text) {
	TextParser parser(text);
	while(!parser.empty()) {
		Str val;
		parser >> val;
		if(isdigit(val[0]))
			return tryFromString<float>(string(val), 0.0f);
	}
	return 0.0f;
}

void initializeGl(GlProfile profile) {
	using Feature = GlFeature;
	using Vendor = GlVendor;
	using Profile = GlProfile;

	int major = 0, minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	s_info.version = float(major) + float(minor) * 0.1f;

#ifdef FWK_TARGET_HTML
	// TODO: which webgl extensions are really required?
	// TODO: turn them into features?
	vector<const char *> must_haves;

	if(major <= 2)
		insertBack(must_haves,
				   {"EXT_shader_texture_lod", "OES_element_index_uint", "OES_texture_float",
					"OES_texture_half_float", "OES_vertex_array_object", "WEBGL_depth_texture",
					"WEBGL_draw_buffers", "OES_standard_derivatives"});

	// TODO: fix it; identify required extensions
	auto context = emscripten_webgl_get_current_context();
	for(auto ext : must_haves)
		if(!emscripten_webgl_enable_extension(context, ext))
			FATAL("OpenGL Extension not supported: %s", ext);
#endif

	auto vendor = toLower((const char *)glGetString(GL_VENDOR));
	if(vendor.find("intel") != string::npos)
		s_info.vendor = Vendor::intel;
	else if(vendor.find("nvidia") != string::npos)
		s_info.vendor = Vendor::nvidia;
	else if(vendor.find("amd") != string::npos)
		s_info.vendor = Vendor::amd;
	else
		s_info.vendor = Vendor::other;
	s_info.profile = profile;
	s_info.renderer = (const char *)glGetString(GL_RENDERER);

	loadExtensions();

	GLint num;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num);
	s_info.extensions.reserve(num);
	s_info.extensions.clear();

#ifdef FWK_TARGET_HTML
#else
	for(int n = 0; n < num; n++) {
		const char *ogl_ext = (const char *)glGetStringi(GL_EXTENSIONS, n);
		if(strncmp(ogl_ext, "GL_", 3) == 0)
			ogl_ext += 3;
		s_info.extensions.emplace_back(ogl_ext);
	}
#endif

	for(auto limit : all<GlLimit>)
		glGetIntegerv(s_limit_map[limit], &s_info.limits[limit]);
	{
		auto *ver = (const char *)glGetString(GL_VERSION);
		auto *glsl_ver = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);

		s_info.version_full = ver;
		s_info.glsl_version_full = glsl_ver;
		s_info.glsl_version = parseOpenglVersion(glsl_ver);
		// TODO: multiple GLSL versions can be supported
	}

	bool core_430 = profile == Profile::core && s_info.version >= 4.3;
	bool core_410 = profile == Profile::core && s_info.version >= 4.1;
	bool core_300 = profile == Profile::core && s_info.version >= 3.0;
	bool core_330 = profile == Profile::core && s_info.version >= 3.3;
	bool es_300 = profile == Profile::es && s_info.version >= 3.0;

	if(core_300 || es_300 || s_info.hasExtension("ARB_vertex_array_object"))
		s_info.features |= Feature::vertex_array_object;
	if(core_430 || s_info.hasExtension("KHR_debug"))
		s_info.features |= Feature::debug;
	if(core_330 || s_info.hasExtension("ARB_timer_query"))
		s_info.features |= Feature::timer_query;
	if(s_info.hasExtension("ARB_copy_image"))
		s_info.features |= Feature::copy_image;
	if(s_info.hasExtension("ARB_texture_view"))
		s_info.features |= Feature::texture_storage;
	if(s_info.hasExtension("ARB_texture_storage"))
		s_info.features |= Feature::texture_storage;
	if(s_info.hasExtension("ARB_shader_draw_parameters"))
		s_info.features |= Feature::shader_draw_parameters;
	if(core_410 || s_info.hasExtension("ARB_separate_shader_objects"))
		s_info.features |= Feature::separate_shader_objects;
	if(s_info.hasExtension("ARB_shader_ballot"))
		s_info.features |= Feature::shader_ballot;
}

void loadExtensions() {
#ifdef FWK_TARGET_MINGW
#define LOAD(func) (func = reinterpret_cast<decltype(func)>((u64)winLoadFunction(#func)));

	LOAD(glCompressedTexImage3D);
	LOAD(glCompressedTexImage2D);
	LOAD(glCompressedTexImage1D);
	LOAD(glCompressedTexSubImage3D);
	LOAD(glCompressedTexSubImage2D);
	LOAD(glCompressedTexSubImage1D);

	LOAD(glBindBuffer);
	LOAD(glDeleteBuffers);
	LOAD(glGenBuffers);
	LOAD(glIsBuffer);
	LOAD(glBufferData);
	LOAD(glBufferSubData);
	LOAD(glGetBufferSubData);
	LOAD(glMapBuffer);
	LOAD(glUnmapBuffer);
	LOAD(glGetBufferParameteriv);
	LOAD(glGetBufferPointerv);
	LOAD(glAttachShader);
	LOAD(glBindAttribLocation);
	LOAD(glCompileShader);

	LOAD(glCreateProgram);
	LOAD(glCreateShader);
	LOAD(glDeleteProgram);
	LOAD(glDeleteShader);
	LOAD(glDetachShader);

	LOAD(glDisableVertexAttribArray);
	LOAD(glEnableVertexAttribArray);
	LOAD(glGetActiveAttrib);
	LOAD(glGetActiveUniform);

	LOAD(glGetAttachedShaders);
	LOAD(glGetAttribLocation);
	LOAD(glGetProgramiv);
	LOAD(glGetProgramInfoLog);
	LOAD(glGetShaderiv);
	LOAD(glGetShaderInfoLog);
	LOAD(glGetShaderSource);
	LOAD(glGetUniformLocation);
	LOAD(glGetUniformfv);
	LOAD(glGetUniformiv);
	LOAD(glGetVertexAttribdv);
	LOAD(glGetVertexAttribfv);
	LOAD(glGetVertexAttribiv);
	LOAD(glGetVertexAttribPointerv);
	LOAD(glLinkProgram);
	LOAD(glShaderSource);
	LOAD(glUseProgram);

	LOAD(glUniform1f);
	LOAD(glUniform2f);
	LOAD(glUniform3f);
	LOAD(glUniform4f);
	LOAD(glUniform1i);
	LOAD(glUniform2i);
	LOAD(glUniform3i);
	LOAD(glUniform4i);
	LOAD(glUniform1ui);
	LOAD(glUniform2ui);
	LOAD(glUniform3ui);
	LOAD(glUniform4ui);

	LOAD(glUniform1fv);
	LOAD(glUniform2fv);
	LOAD(glUniform3fv);
	LOAD(glUniform4fv);
	LOAD(glUniform1iv);
	LOAD(glUniform2iv);
	LOAD(glUniform3iv);
	LOAD(glUniform4iv);
	LOAD(glUniform1uiv);
	LOAD(glUniform2uiv);
	LOAD(glUniform3uiv);
	LOAD(glUniform4uiv);

	LOAD(glUniformMatrix2fv);
	LOAD(glUniformMatrix3fv);
	LOAD(glUniformMatrix4fv);

	LOAD(glProgramUniform1f);
	LOAD(glProgramUniform2f);
	LOAD(glProgramUniform3f);
	LOAD(glProgramUniform4f);
	LOAD(glProgramUniform1i);
	LOAD(glProgramUniform2i);
	LOAD(glProgramUniform3i);
	LOAD(glProgramUniform4i);
	LOAD(glProgramUniform1ui);
	LOAD(glProgramUniform2ui);
	LOAD(glProgramUniform3ui);
	LOAD(glProgramUniform4ui);

	LOAD(glProgramUniform1fv);
	LOAD(glProgramUniform2fv);
	LOAD(glProgramUniform3fv);
	LOAD(glProgramUniform4fv);
	LOAD(glProgramUniform1iv);
	LOAD(glProgramUniform2iv);
	LOAD(glProgramUniform3iv);
	LOAD(glProgramUniform4iv);
	LOAD(glProgramUniform1uiv);
	LOAD(glProgramUniform2uiv);
	LOAD(glProgramUniform3uiv);
	LOAD(glProgramUniform4uiv);

	LOAD(glProgramUniformMatrix2fv);
	LOAD(glProgramUniformMatrix3fv);
	LOAD(glProgramUniformMatrix4fv);

	LOAD(glValidateProgram);
	LOAD(glVertexAttribPointer);
	LOAD(glGenerateMipmap);
	LOAD(glBindVertexArray);
	LOAD(glDeleteVertexArrays);
	LOAD(glGenVertexArrays);
	LOAD(glVertexAttrib4f);
	LOAD(glActiveTexture);

	LOAD(glBindRenderbuffer);
	LOAD(glDeleteRenderbuffers);
	LOAD(glGenRenderbuffers);
	LOAD(glRenderbufferStorage);
	LOAD(glRenderbufferStorageMultisample);
	LOAD(glBlitFramebuffer);

	LOAD(glBindFramebuffer);
	LOAD(glDeleteFramebuffers);
	LOAD(glGenFramebuffers);
	LOAD(glCheckFramebufferStatus);
	LOAD(glFramebufferRenderbuffer);
	LOAD(glFramebufferTexture2D);
	LOAD(glDrawBuffers);

	LOAD(glClearTexImage);
	LOAD(glClearTexSubImage);

	LOAD(glBindImageTexture);
	LOAD(glMemoryBarrier);

	LOAD(glClearBufferData);
	LOAD(glClearBufferSubData);

	LOAD(glBindBufferRange);
	LOAD(glBindBufferBase);

	LOAD(glDispatchCompute);
	LOAD(glDispatchComputeIndirect);

	LOAD(glClearBufferiv);
	LOAD(glClearBufferuiv);
	LOAD(glClearBufferfv);
	LOAD(glClearBufferfi);

	LOAD(glBlendEquation);

	LOAD(glDebugMessageCallback);
	LOAD(glDebugMessageControl);
	LOAD(glDebugMessageInsert);

	LOAD(glGenQueries);
	LOAD(glDeleteQueries);
	LOAD(glIsQuery);
	LOAD(glBeginQuery);
	LOAD(glEndQuery);
	LOAD(glGetQueryiv);
	LOAD(glGetQueryObjectiv);
	LOAD(glGetQueryObjectuiv);

	LOAD(glCopyImageSubData);
	LOAD(glTextureView);
	LOAD(glTexStorage1D);
	LOAD(glTexStorage2D);
	LOAD(glTexStorage3D);
	LOAD(glTexStorage2DMultisample);
	LOAD(glTexImage2DMultisample);

	LOAD(glGetProgramBinary);
	LOAD(glGetProgramInterfaceiv);
	LOAD(glGetProgramResourceName);
	LOAD(glGetProgramResourceiv);

	LOAD(glDrawArraysInstanced);
	LOAD(glDrawElementsInstanced);
	LOAD(glMultiDrawArraysIndirect);
	LOAD(glVertexAttribIPointer);

	LOAD(glBufferStorage);
	LOAD(glInvalidateBufferSubData);
	LOAD(glInvalidateBufferData);
	LOAD(glMapBufferRange);
	LOAD(glFlushMappedBufferRange);
	LOAD(glCopyBufferSubData);

	LOAD(glGenTransformFeedbacks);
	LOAD(glGenProgramPipelines);
	LOAD(glGenSamplers);
	LOAD(glDeleteSamplers);
	LOAD(glDeleteProgramPipelines);
	LOAD(glDeleteTransformFeedbacks);

	LOAD(glGetStringi);
	LOAD(glBindSampler)
	LOAD(glBlendEquationSeparate)
	LOAD(glBlendFuncSeparate)

	LOAD(glVertexAttribI1i)
	LOAD(glProvokingVertex)

#undef LOAD
#endif
}

// TODO: think of better error handling
void testGlError(const char *msg) {
#ifndef FWK_TARGET_HTML5
	int err = glGetError();
	if(err == GL_NO_ERROR)
		return;

	const char *err_code = "unknown";

	switch(err) {
#define CASE(e)                                                                                    \
	case e:                                                                                        \
		err_code = #e;                                                                             \
		break;
		CASE(GL_INVALID_ENUM)
		CASE(GL_INVALID_VALUE)
		CASE(GL_INVALID_OPERATION)
		//			CASE(GL_INVALID_FRAMEBUFFER_OPERATION)
		CASE(GL_STACK_OVERFLOW)
		CASE(GL_STACK_UNDERFLOW)
		CASE(GL_OUT_OF_MEMORY)
	default:
		break;
#undef CASE
	}

	FATAL("%s: %s", msg, err_code);
#endif
}

static const char *debugSourceText(GLenum source) {
	switch(source) {
#define CASE(suffix, text)                                                                         \
	case GL_DEBUG_SOURCE_##suffix:                                                                 \
		return text;
		CASE(API, "API")
		CASE(WINDOW_SYSTEM, "window system")
		CASE(SHADER_COMPILER, "shader compiler")
		CASE(THIRD_PARTY, "third party")
		CASE(APPLICATION, "application")
		CASE(OTHER, "other")
#undef CASE
	}

	return "unknown";
}

static const char *debugTypeText(GLenum type) {
	switch(type) {
#define CASE(suffix, text)                                                                         \
	case GL_DEBUG_TYPE_##suffix:                                                                   \
		return text;
		CASE(ERROR, "error")
		CASE(DEPRECATED_BEHAVIOR, "deprecated behavior")
		CASE(UNDEFINED_BEHAVIOR, "undefined behavior")
		CASE(PORTABILITY, "portability issue")
		CASE(PERFORMANCE, "performance issue")
		CASE(MARKER, "marker")
		CASE(PUSH_GROUP, "push group")
		CASE(POP_GROUP, "pop group")
		CASE(OTHER, "other issue")
#undef CASE
	}

	return "unknown";
}

static const char *debugSeverityText(GLenum severity) {
	switch(severity) {
#define CASE(suffix, text)                                                                         \
	case GL_DEBUG_SEVERITY_##suffix:                                                               \
		return text;
		CASE(HIGH, "HIGH")
		CASE(MEDIUM, "medium")
		CASE(LOW, "low")
		CASE(NOTIFICATION, "notification")
#undef CASE
	}

	return "unknown";
}

static void APIENTRY debugOutputCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
										 GLsizei length, const GLchar *message,
										 const void *userParam) {
	// ignore non-significant error/warning codes
	if(s_info.vendor == GlVendor::nvidia) {
		if(id == 131169 || id == 131185 || id == 131218 || id == 131204)
			return;
		if(id == 131186) // Buffer is being copied/moved from VIDEO memory to HOST memory
			return;
	} else if(s_info.vendor == GlVendor::intel) {
		Str msg((const char *)message);
		if(msg.find("warning: extension") != -1 && msg.find("unsupported in") != -1)
			return;
		if(source == GL_DEBUG_SOURCE_SHADER_COMPILER)
			return; // It's handled in GlShader & GlProgram
	}

	TextFormatter fmt;
	fmt("Opengl % [%] ID:% Source:%\n", debugTypeText(type), debugSeverityText(severity), id,
		debugSourceText(source));
	fmt("%", message);

	if(severity == GL_DEBUG_SEVERITY_HIGH && type != GL_DEBUG_TYPE_OTHER)
		FATAL("%s", fmt.text().c_str());
	log(fmt.text());
}

bool installGlDebugHandler() {
#ifdef FWK_TARGET_HTML
	return false;
#else
	if(!s_info.hasFeature(GlFeature::debug))
		return false;

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(debugOutputCallback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
						  GL_FALSE);
	return true;
#endif
}

void clearColor(FColor col) {
	glClearColor(col.r, col.g, col.b, col.a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void clearColor(IColor col) { clearColor(FColor(col)); }

void clearDepth(float value) {
	glClearDepth(value);
	glDepthMask(1);
	glClear(GL_DEPTH_BUFFER_BIT);
}
}

#undef EXT_API
