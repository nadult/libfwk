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
#include "fwk/str.h"
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace fwk {

void *winLoadFunction(const char *name);

static EnumMap<OpenglExtension, bool> s_is_extension_supported;

using ExtNames = EnumMap<OpenglExtension, const char *>;
static ExtNames s_ext_names = {"EXT_texture_filter_anisotropic",
							   "WEBGL_compressed_texture_s3tc",
							   "NV_conservative_raster",
							   "KHR_debug",
							   "ARB_timer_query",
							   "ARB_copy_image",
							   "ARB_texture_view",
							   "ARB_texture_storage"};

static OpenglVendor s_opengl_vendor;

OpenglVendor openglVendor() { return s_opengl_vendor; }
const char *openglName(OpenglExtension ext) { return s_ext_names[ext]; }
bool isExtensionSupported(OpenglExtension ext) { return s_is_extension_supported[ext]; }

void initializeOpenGL() {
#ifdef __EMSCRIPTEN__
	const char *must_haves[] = {"EXT_shader_texture_lod",   "OES_element_index_uint",
								"OES_standard_derivatives", "OES_texture_float",
								"OES_texture_half_float",   "OES_vertex_array_object",
								"WEBGL_depth_texture",		"WEBGL_draw_buffers"};

	auto context = emscripten_webgl_get_current_context();
	for(auto ext : must_haves)
		if(!emscripten_webgl_enable_extension(context, ext))
			FATAL("OpenGL Extension not supported: %s", ext);
	for(auto ext : all<OpenglExtension>())
		emscripten_webgl_enable_extension(context, s_ext_names[ext]);
#else
	int major = 0, minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	int version = major * 100 + minor;
	if(version < 300)
		FATAL("Minimum required OpenGL version: 3.0");
#endif

	auto vendor = toLower((const char *)glGetString(GL_VENDOR));
	if(vendor.find("intel") != string::npos)
		s_opengl_vendor = OpenglVendor::intel;
	else if(vendor.find("nvidia") != string::npos)
		s_opengl_vendor = OpenglVendor::nvidia;
	else if(vendor.find("amd") != string::npos)
		s_opengl_vendor = OpenglVendor::amd;
	else
		s_opengl_vendor = OpenglVendor::other;

	auto ext_strings = (const char *)glGetString(GL_EXTENSIONS);
	for(auto ext : all<OpenglExtension>())
		s_is_extension_supported[ext] = strstr(ext_strings, s_ext_names[ext]) != nullptr;

#ifdef FWK_TARGET_MINGW
#define LOAD(func) (func = (decltype(func))winLoadFunction(#func));

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
	LOAD(glUniform1fv);
	LOAD(glUniform2fv);
	LOAD(glUniform3fv);
	LOAD(glUniform4fv);
	LOAD(glUniform1iv);
	LOAD(glUniform2iv);
	LOAD(glUniform3iv);
	LOAD(glUniform4iv);
	LOAD(glUniformMatrix2fv);
	LOAD(glUniformMatrix3fv);
	LOAD(glUniformMatrix4fv);

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
	if(s_opengl_vendor == OpenglVendor::nvidia) {
		if(id == 131169 || id == 131185 || id == 131218 || id == 131204)
			return;
	} else if(s_opengl_vendor == OpenglVendor::intel) {
		Str msg((const char *)message);
		if(msg.find("warning: extension") != -1 && msg.find("unsupported in") != -1)
			return;
	}

	TextFormatter fmt;
	fmt("Opengl % [%] ID:% Source:%\n", debugTypeText(type), debugSeverityText(severity), id,
		debugSourceText(source));
	fmt("%", message);

	if(severity == GL_DEBUG_SEVERITY_HIGH && type != GL_DEBUG_TYPE_OTHER)
		FATAL("%s", fmt.text().c_str());
	print("%\n", fmt.text());
}

bool installOpenglDebugHandler() {
	if(!isExtensionSupported(OpenglExtension::debug))
		return false;

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(debugOutputCallback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
						  GL_FALSE);
	return true;
}
}

#undef EXT_API
