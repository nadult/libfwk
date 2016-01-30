/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifdef FWK_OPENGL_H
#define FWK_OPENGL_H_ONLY_EXTENSIONS
#undef FWK_OPENGL_H
#endif

#define EXT_API

#include "fwk.h"
#include "fwk_opengl.h"
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#endif

namespace fwk {

#ifdef _WIN32
static PROC loadFunction(const char *name) {
	PROC func = wglGetProcAddress(name);
	if(!func)
		THROW("Error while importing OpenGL function: %s", name);
	return func;
}
#endif

static bool s_is_extension_supported[count<OpenglExtension>()] = {
	false,
};

void initializeOpenGL() {
#ifdef __EMSCRIPTEN__
	const char *must_haves[] = {"EXT_shader_texture_lod",   "OES_element_index_uint",
								"OES_standard_derivatives", "OES_texture_float",
								"OES_texture_half_float",   "OES_vertex_array_object",
								"WEBGL_depth_texture",		"WEBGL_draw_buffers"};
	const char *optionals[] = {"EXT_texture_filter_anisotropic", "WEBGL_compressed_texture_s3tc"};

	auto context = emscripten_webgl_get_current_context();
	for(auto ext : must_haves)
		if(!emscripten_webgl_enable_extension(context, ext))
			THROW("OpenGL Extension not supported: %s", ext);
	for(auto ext : optionals)
		emscripten_webgl_enable_extension(context, ext);
#else
	int major = 0, minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	int version = major * 100 + minor;
	if(version < 300)
		THROW("Minimum required OpenGL version: 3.0");
#endif

	const char *strings = (const char *)glGetString(GL_EXTENSIONS);
	for(auto elem : all<OpenglExtension>())
		s_is_extension_supported[elem] = strstr(strings, toString(elem)) != nullptr;

#ifdef _WIN32
#define LOAD(func) (func = (decltype(func))loadFunction(#func));
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

	THROW("%s: %s", msg, err_code);
#endif
}

bool isExtensionSupported(OpenglExtension ext) { return s_is_extension_supported[ext]; }

void glColor(Color color) { glColor4ub(color.r, color.g, color.b, color.a); }

void glVertex(const float2 &v) { glVertex2f(v.x, v.y); }

void glTexCoord(const float2 &v) { glTexCoord2f(v.x, v.y); }
}

#undef EXT_API
