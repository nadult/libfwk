// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_GFX_OPENGL_H
#define FWK_GFX_OPENGL_H

#ifdef __MINGW32__
#ifndef _WINDOWS_
#define _WINDOWS_
#define APIENTRY __attribute__((__stdcall__))
#define WINGDIAPI __attribute__((dllimport))
#endif
#else
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <GL/gl.h>
#include <GL/glext.h>

#ifdef __MINGW32__

#ifndef EXT_API
#define EXT_API extern
#endif

#define EXT_ENTRY __stdcall

extern "C" {

EXT_API void(EXT_ENTRY *glCompressedTexImage3D)(GLenum target, GLint level, GLenum internalformat,
												GLsizei width, GLsizei height, GLsizei depth,
												GLint border, GLsizei imageSize,
												const GLvoid *data);
EXT_API void(EXT_ENTRY *glCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat,
												GLsizei width, GLsizei height, GLint border,
												GLsizei imageSize, const GLvoid *data);
EXT_API void(EXT_ENTRY *glCompressedTexImage1D)(GLenum target, GLint level, GLenum internalformat,
												GLsizei width, GLint border, GLsizei imageSize,
												const GLvoid *data);
EXT_API void(EXT_ENTRY *glCompressedTexSubImage3D)(GLenum target, GLint level, GLint xoffset,
												   GLint yoffset, GLint zoffset, GLsizei width,
												   GLsizei height, GLsizei depth, GLenum format,
												   GLsizei imageSize, const GLvoid *data);
EXT_API void(EXT_ENTRY *glCompressedTexSubImage2D)(GLenum target, GLint level, GLint xoffset,
												   GLint yoffset, GLsizei width, GLsizei height,
												   GLenum format, GLsizei imageSize,
												   const GLvoid *data);
EXT_API void(EXT_ENTRY *glCompressedTexSubImage1D)(GLenum target, GLint level, GLint xoffset,
												   GLsizei width, GLenum format, GLsizei imageSize,
												   const GLvoid *data);

EXT_API void(EXT_ENTRY *glBindBuffer)(GLenum target, GLuint buffer);
EXT_API void(EXT_ENTRY *glDeleteBuffers)(GLsizei n, const GLuint *buffers);
EXT_API void(EXT_ENTRY *glGenBuffers)(GLsizei n, GLuint *buffers);
EXT_API GLboolean(EXT_ENTRY *glIsBuffer)(GLuint buffer);
EXT_API void(EXT_ENTRY *glBufferData)(GLenum target, GLsizeiptr size, const GLvoid *data,
									  GLenum usage);
EXT_API void(EXT_ENTRY *glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size,
										 const GLvoid *data);
EXT_API void(EXT_ENTRY *glGetBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size,
											GLvoid *data);
EXT_API GLvoid *(EXT_ENTRY *glMapBuffer)(GLenum target, GLenum access);
EXT_API GLboolean(EXT_ENTRY *glUnmapBuffer)(GLenum target);
EXT_API void(EXT_ENTRY *glGetBufferParameteriv)(GLenum target, GLenum pname, GLint *params);
EXT_API void(EXT_ENTRY *glGetBufferPointerv)(GLenum target, GLenum pname, GLvoid **params);

EXT_API void(EXT_ENTRY *glAttachShader)(GLuint program, GLuint shader);
EXT_API void(EXT_ENTRY *glBindAttribLocation)(GLuint program, GLuint index, const GLchar *name);
EXT_API void(EXT_ENTRY *glCompileShader)(GLuint shader);
EXT_API GLuint(EXT_ENTRY *glCreateProgram)(void);
EXT_API GLuint(EXT_ENTRY *glCreateShader)(GLenum type);
EXT_API void(EXT_ENTRY *glDeleteProgram)(GLuint program);
EXT_API void(EXT_ENTRY *glDeleteShader)(GLuint shader);
EXT_API void(EXT_ENTRY *glDetachShader)(GLuint program, GLuint shader);
EXT_API void(EXT_ENTRY *glDisableVertexAttribArray)(GLuint index);
EXT_API void(EXT_ENTRY *glEnableVertexAttribArray)(GLuint index);
EXT_API void(EXT_ENTRY *glGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufSize,
										   GLsizei *length, GLint *size, GLenum *type,
										   GLchar *name);
EXT_API void(EXT_ENTRY *glGetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize,
											GLsizei *length, GLint *size, GLenum *type,
											GLchar *name);
EXT_API void(EXT_ENTRY *glGetAttachedShaders)(GLuint program, GLsizei maxCount, GLsizei *count,
											  GLuint *obj);
EXT_API GLint(EXT_ENTRY *glGetAttribLocation)(GLuint program, const GLchar *name);
EXT_API void(EXT_ENTRY *glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
EXT_API void(EXT_ENTRY *glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length,
											 GLchar *infoLog);
EXT_API void(EXT_ENTRY *glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
EXT_API void(EXT_ENTRY *glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length,
											GLchar *infoLog);
EXT_API void(EXT_ENTRY *glGetShaderSource)(GLuint shader, GLsizei bufSize, GLsizei *length,
										   GLchar *source);
EXT_API GLint(EXT_ENTRY *glGetUniformLocation)(GLuint program, const GLchar *name);
EXT_API void(EXT_ENTRY *glGetUniformfv)(GLuint program, GLint location, GLfloat *params);
EXT_API void(EXT_ENTRY *glGetUniformiv)(GLuint program, GLint location, GLint *params);
EXT_API void(EXT_ENTRY *glGetVertexAttribdv)(GLuint index, GLenum pname, GLdouble *params);
EXT_API void(EXT_ENTRY *glGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
EXT_API void(EXT_ENTRY *glGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params);
EXT_API void(EXT_ENTRY *glGetVertexAttribPointerv)(GLuint index, GLenum pname, GLvoid **pointer);

EXT_API void(EXT_ENTRY *glLinkProgram)(GLuint program);
EXT_API void(EXT_ENTRY *glShaderSource)(GLuint shader, GLsizei count, const GLchar *const *string,
										const GLint *length);
EXT_API void(EXT_ENTRY *glUseProgram)(GLuint program);

EXT_API void(EXT_ENTRY *glUniform1f)(GLint location, GLfloat v0);
EXT_API void(EXT_ENTRY *glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
EXT_API void(EXT_ENTRY *glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
EXT_API void(EXT_ENTRY *glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
									 GLfloat v3);
EXT_API void(EXT_ENTRY *glUniform1i)(GLint location, GLint v0);
EXT_API void(EXT_ENTRY *glUniform2i)(GLint location, GLint v0, GLint v1);
EXT_API void(EXT_ENTRY *glUniform3i)(GLint location, GLint v0, GLint v1, GLint v2);
EXT_API void(EXT_ENTRY *glUniform4i)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
EXT_API void(EXT_ENTRY *glUniform1fv)(GLint location, GLsizei count, const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniform2fv)(GLint location, GLsizei count, const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniform3fv)(GLint location, GLsizei count, const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniform4fv)(GLint location, GLsizei count, const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniform1iv)(GLint location, GLsizei count, const GLint *value);
EXT_API void(EXT_ENTRY *glUniform2iv)(GLint location, GLsizei count, const GLint *value);
EXT_API void(EXT_ENTRY *glUniform3iv)(GLint location, GLsizei count, const GLint *value);
EXT_API void(EXT_ENTRY *glUniform4iv)(GLint location, GLsizei count, const GLint *value);
EXT_API void(EXT_ENTRY *glUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose,
											const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose,
											const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose,
											const GLfloat *value);

EXT_API void(EXT_ENTRY *glValidateProgram)(GLuint program);
EXT_API void(EXT_ENTRY *glVertexAttribPointer)(GLuint index, GLint size, GLenum type,
											   GLboolean normalized, GLsizei stride,
											   const GLvoid *pointer);
EXT_API void(EXT_ENTRY *glGenerateMipmap)(GLenum target);
EXT_API void(EXT_ENTRY *glBindVertexArray)(GLuint array);
EXT_API void(EXT_ENTRY *glDeleteVertexArrays)(GLsizei n, const GLuint *arrays);
EXT_API void(EXT_ENTRY *glGenVertexArrays)(GLsizei n, GLuint *arrays);
EXT_API void(EXT_ENTRY *glVertexAttrib4f)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
EXT_API void(EXT_ENTRY *glActiveTexture)(GLenum texture);

EXT_API void(EXT_ENTRY *glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
EXT_API void(EXT_ENTRY *glDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
EXT_API void(EXT_ENTRY *glGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
EXT_API void(EXT_ENTRY *glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width,
											   GLsizei height);

EXT_API void(EXT_ENTRY *glBindFramebuffer)(GLenum target, GLuint framebuffer);
EXT_API void(EXT_ENTRY *glDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
EXT_API void(EXT_ENTRY *glGenFramebuffers)(GLsizei n, GLuint *framebuffers);
EXT_API GLenum(EXT_ENTRY *glCheckFramebufferStatus)(GLenum target);
EXT_API void(EXT_ENTRY *glFramebufferRenderbuffer)(GLenum target, GLenum attachment,
												   GLenum renderbuffertarget, GLuint renderbuffer);
EXT_API void(EXT_ENTRY *glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget,
												GLuint texture, GLint level);
EXT_API void(EXT_ENTRY *glDrawBuffers)(GLsizei n, const GLenum *bufs);

EXT_API void(EXT_ENTRY *glClearTexImage)(GLuint texture, GLint level, GLenum format, GLenum type,
										 const void *data);
EXT_API void(EXT_ENTRY *glClearTexSubImage)(GLuint texture, GLint level, GLint xoffset,
											GLint yoffset, GLint zoffset, GLsizei width,
											GLsizei height, GLsizei depth, GLenum format,
											GLenum type, const void *data);

EXT_API void(EXT_ENTRY *glBindImageTexture)(GLuint unit, GLuint texture, GLint level,
											GLboolean layered, GLint layer, GLenum access,
											GLenum format);
EXT_API void(EXT_ENTRY *glMemoryBarrier)(GLbitfield barriers);

EXT_API void(EXT_ENTRY *glClearBufferData)(GLenum target, GLenum internalformat, GLenum format,
										   GLenum type, const void *data);
EXT_API void(EXT_ENTRY *glClearBufferSubData)(GLenum target, GLenum internalformat, GLintptr offset,
											  GLsizeiptr size, GLenum format, GLenum type,
											  const void *data);

EXT_API void(EXT_ENTRY *glBindBufferRange)(GLenum target, GLuint index, GLuint buffer,
										   GLintptr offset, GLsizeiptr size);
EXT_API void(EXT_ENTRY *glBindBufferBase)(GLenum target, GLuint index, GLuint buffer);

EXT_API void(EXT_ENTRY *glDispatchCompute)(GLuint num_groups_x, GLuint num_groups_y,
										   GLuint num_groups_z);
EXT_API void(EXT_ENTRY *glDispatchComputeIndirect)(GLintptr indirect);

EXT_API void(EXT_ENTRY *glClearBufferiv)(GLenum buffer, GLint drawbuffer, const GLint *value);
EXT_API void(EXT_ENTRY *glClearBufferuiv)(GLenum buffer, GLint drawbuffer, const GLuint *value);
EXT_API void(EXT_ENTRY *glClearBufferfv)(GLenum buffer, GLint drawbuffer, const GLfloat *value);
EXT_API void(EXT_ENTRY *glClearBufferfi)(GLenum buffer, GLint drawbuffer, GLfloat depth,
										 GLint stencil);

EXT_API void(EXT_ENTRY *glBlendEquation)(GLenum mode);
}

#undef EXT_API
#undef EXT_ENTRY

#endif

#ifndef FWK_GFX_OPENGL_H_ONLY_EXTENSIONS

#include "fwk/gfx_base.h"

namespace fwk {

void testGlError(const char *);
bool installOpenglDebugHandler();

DEFINE_ENUM(OpenglExtension, compressed_texture_s3tc, texture_filter_anisotropic,
			nv_conservative_raster, debug, timer_query);

const char *glName(OpenglExtension);
bool isExtensionSupported(OpenglExtension);
}

#endif

#endif
