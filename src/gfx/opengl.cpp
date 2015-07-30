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

namespace fwk {

#ifdef _WIN32
static PROC loadFunction(const char *name) {
	PROC func = wglGetProcAddress(name);
	//		if(!func)
	//			THROW("Error while importing OpenGL function: %s", name);
	return func;
}
#endif

DEFINE_ENUM(OpenglExtension, "EXT_texture_compression_dxt1", "EXT_texture_compression_s3tc",
			"ARB_texture_non_power_of_two");

static bool s_is_extension_supported[OpenglExtension::count] = {
	false,
};

void loadExtensions() {
	const char *strings = (const char *)glGetString(GL_EXTENSIONS);
	for(int n = 0; n < OpenglExtension::count; n++)
		s_is_extension_supported[n] =
			strstr(strings, OpenglExtension::toString((OpenglExtension::Type)n)) != nullptr;

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

bool isExtensionSupported(OpenglExtension::Type ext) { return s_is_extension_supported[ext]; }

void glColor(Color color) { glColor4ub(color.r, color.g, color.b, color.a); }

void glVertex(const float2 &v) { glVertex2f(v.x, v.y); }

void glTexCoord(const float2 &v) { glTexCoord2f(v.x, v.y); }
}

#undef EXT_API
