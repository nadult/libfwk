// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_GFX_OPENGL_H
#define FWK_GFX_OPENGL_H

#include "fwk/sys/platform.h"

#if defined(FWK_PLATFORM_MINGW) || defined(FWK_PLATFORM_MSVC)
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

#if defined(FWK_PLATFORM_MINGW) || defined(FWK_PLATFORM_MSVC)

#ifndef EXT_API
#define EXT_API extern
#endif

#define EXT_ENTRY __stdcall

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
EXT_API void(EXT_ENTRY *glGetQueryObjecti64v)(GLuint id, GLenum pname, GLint64 *params);
EXT_API void(EXT_ENTRY *glGetQueryObjectui64v)(GLuint id, GLenum pname, GLuint64 *params);
EXT_API void(EXT_ENTRY *glGetIntegeri_v)(GLenum target, GLuint index, GLint *data);

EXT_API void(EXT_ENTRY *glLinkProgram)(GLuint program);
EXT_API void(EXT_ENTRY *glShaderSource)(GLuint shader, GLsizei count, const GLchar *const *string,
										const GLint *length);
EXT_API void(EXT_ENTRY *glUseProgram)(GLuint program);

EXT_API void(EXT_ENTRY *glUniform1f)(GLint location, GLfloat);
EXT_API void(EXT_ENTRY *glUniform2f)(GLint location, GLfloat, GLfloat);
EXT_API void(EXT_ENTRY *glUniform3f)(GLint location, GLfloat, GLfloat, GLfloat);
EXT_API void(EXT_ENTRY *glUniform4f)(GLint location, GLfloat, GLfloat, GLfloat, GLfloat);
EXT_API void(EXT_ENTRY *glUniform1i)(GLint location, GLint);
EXT_API void(EXT_ENTRY *glUniform2i)(GLint location, GLint, GLint);
EXT_API void(EXT_ENTRY *glUniform3i)(GLint location, GLint, GLint, GLint);
EXT_API void(EXT_ENTRY *glUniform4i)(GLint location, GLint, GLint, GLint, GLint);
EXT_API void(EXT_ENTRY *glUniform1ui)(GLint location, GLuint);
EXT_API void(EXT_ENTRY *glUniform2ui)(GLint location, GLuint, GLuint);
EXT_API void(EXT_ENTRY *glUniform3ui)(GLint location, GLuint, GLuint, GLuint);
EXT_API void(EXT_ENTRY *glUniform4ui)(GLint location, GLuint, GLuint, GLuint, GLuint);

EXT_API void(EXT_ENTRY *glUniform1fv)(GLint location, GLsizei count, const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniform2fv)(GLint location, GLsizei count, const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniform3fv)(GLint location, GLsizei count, const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniform4fv)(GLint location, GLsizei count, const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniform1iv)(GLint location, GLsizei count, const GLint *value);
EXT_API void(EXT_ENTRY *glUniform2iv)(GLint location, GLsizei count, const GLint *value);
EXT_API void(EXT_ENTRY *glUniform3iv)(GLint location, GLsizei count, const GLint *value);
EXT_API void(EXT_ENTRY *glUniform4iv)(GLint location, GLsizei count, const GLint *value);
EXT_API void(EXT_ENTRY *glUniform1uiv)(GLint location, GLsizei count, const GLuint *value);
EXT_API void(EXT_ENTRY *glUniform2uiv)(GLint location, GLsizei count, const GLuint *value);
EXT_API void(EXT_ENTRY *glUniform3uiv)(GLint location, GLsizei count, const GLuint *value);
EXT_API void(EXT_ENTRY *glUniform4uiv)(GLint location, GLsizei count, const GLuint *value);

EXT_API void(EXT_ENTRY *glUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose,
											const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose,
											const GLfloat *value);
EXT_API void(EXT_ENTRY *glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose,
											const GLfloat *value);

EXT_API void(EXT_ENTRY *glProgramUniform1f)(GLuint program, GLint loc, GLfloat);
EXT_API void(EXT_ENTRY *glProgramUniform2f)(GLuint program, GLint loc, GLfloat, GLfloat);
EXT_API void(EXT_ENTRY *glProgramUniform3f)(GLuint program, GLint loc, GLfloat, GLfloat, GLfloat);
EXT_API void(EXT_ENTRY *glProgramUniform4f)(GLuint program, GLint loc, GLfloat, GLfloat, GLfloat,
											GLfloat);
EXT_API void(EXT_ENTRY *glProgramUniform1i)(GLuint program, GLint loc, GLint);
EXT_API void(EXT_ENTRY *glProgramUniform2i)(GLuint program, GLint loc, GLint, GLint);
EXT_API void(EXT_ENTRY *glProgramUniform3i)(GLuint program, GLint loc, GLint, GLint, GLint);
EXT_API void(EXT_ENTRY *glProgramUniform4i)(GLuint program, GLint loc, GLint, GLint, GLint, GLint);
EXT_API void(EXT_ENTRY *glProgramUniform1ui)(GLuint program, GLint loc, GLuint);
EXT_API void(EXT_ENTRY *glProgramUniform2ui)(GLuint program, GLint loc, GLuint, GLuint);
EXT_API void(EXT_ENTRY *glProgramUniform3ui)(GLuint program, GLint loc, GLuint, GLuint, GLuint);
EXT_API void(EXT_ENTRY *glProgramUniform4ui)(GLuint program, GLint loc, GLuint, GLuint, GLuint,
											 GLuint);

EXT_API void(EXT_ENTRY *glProgramUniform1fv)(GLuint program, GLint location, GLsizei count,
											 const GLfloat *value);
EXT_API void(EXT_ENTRY *glProgramUniform2fv)(GLuint program, GLint location, GLsizei count,
											 const GLfloat *value);
EXT_API void(EXT_ENTRY *glProgramUniform3fv)(GLuint program, GLint location, GLsizei count,
											 const GLfloat *value);
EXT_API void(EXT_ENTRY *glProgramUniform4fv)(GLuint program, GLint location, GLsizei count,
											 const GLfloat *value);
EXT_API void(EXT_ENTRY *glProgramUniform1iv)(GLuint program, GLint location, GLsizei count,
											 const GLint *value);
EXT_API void(EXT_ENTRY *glProgramUniform2iv)(GLuint program, GLint location, GLsizei count,
											 const GLint *value);
EXT_API void(EXT_ENTRY *glProgramUniform3iv)(GLuint program, GLint location, GLsizei count,
											 const GLint *value);
EXT_API void(EXT_ENTRY *glProgramUniform4iv)(GLuint program, GLint location, GLsizei count,
											 const GLint *value);
EXT_API void(EXT_ENTRY *glProgramUniform1uiv)(GLuint program, GLint location, GLsizei count,
											  const GLuint *value);
EXT_API void(EXT_ENTRY *glProgramUniform2uiv)(GLuint program, GLint location, GLsizei count,
											  const GLuint *value);
EXT_API void(EXT_ENTRY *glProgramUniform3uiv)(GLuint program, GLint location, GLsizei count,
											  const GLuint *value);
EXT_API void(EXT_ENTRY *glProgramUniform4uiv)(GLuint program, GLint location, GLsizei count,
											  const GLuint *value);

EXT_API void(EXT_ENTRY *glProgramUniformMatrix2fv)(GLuint program, GLint location, GLsizei count,
												   GLboolean transpose, const GLfloat *value);
EXT_API void(EXT_ENTRY *glProgramUniformMatrix3fv)(GLuint program, GLint location, GLsizei count,
												   GLboolean transpose, const GLfloat *value);
EXT_API void(EXT_ENTRY *glProgramUniformMatrix4fv)(GLuint program, GLint location, GLsizei count,
												   GLboolean transpose, const GLfloat *value);

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
EXT_API void(EXT_ENTRY *glRenderbufferStorageMultisample)(GLenum target, GLsizei samples,
														  GLenum internalformat, GLsizei width,
														  GLsizei height);
EXT_API void(EXT_ENTRY *glBlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
										   GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
										   GLbitfield mask, GLenum filter);

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

EXT_API void(EXT_ENTRY *glDebugMessageCallback)(GLDEBUGPROC callback, const void *userParam);
EXT_API void(EXT_ENTRY *glDebugMessageControl)(GLenum source, GLenum type, GLenum severity,
											   GLsizei count, const GLuint *ids, GLboolean enabled);
EXT_API void(EXT_ENTRY *glDebugMessageInsert)(GLenum source, GLenum type, GLuint id,
											  GLenum severity, GLsizei length, const GLchar *buf);

EXT_API void(EXT_ENTRY *glGenQueries)(GLsizei n, GLuint *ids);
EXT_API void(EXT_ENTRY *glDeleteQueries)(GLsizei n, const GLuint *ids);
EXT_API GLboolean(EXT_ENTRY *glIsQuery)(GLuint id);
EXT_API void(EXT_ENTRY *glBeginQuery)(GLenum target, GLuint id);
EXT_API void(EXT_ENTRY *glEndQuery)(GLenum target);
EXT_API void(EXT_ENTRY *glGetQueryiv)(GLenum target, GLenum pname, GLint *params);
EXT_API void(EXT_ENTRY *glGetQueryObjectiv)(GLuint id, GLenum pname, GLint *params);
EXT_API void(EXT_ENTRY *glGetQueryObjectuiv)(GLuint id, GLenum pname, GLuint *params);

EXT_API void(EXT_ENTRY *glCopyImageSubData)(GLuint srcName, GLenum srcTarget, GLint srcLevel,
											GLint srcX, GLint srcY, GLint srcZ, GLuint dstName,
											GLenum dstTarget, GLint dstLevel, GLint dstX,
											GLint dstY, GLint dstZ, GLsizei srcWidth,
											GLsizei srcHeight, GLsizei srcDepth);

EXT_API void(EXT_ENTRY *glTextureView)(GLuint texture, GLenum target, GLuint origtexture,
									   GLenum internalformat, GLuint minlevel, GLuint numlevels,
									   GLuint minlayer, GLuint numlayers);
EXT_API void(EXT_ENTRY *glTexStorage1D)(GLenum target, GLsizei levels, GLenum internalformat,
										GLsizei width);
EXT_API void(EXT_ENTRY *glTexStorage2D)(GLenum target, GLsizei levels, GLenum internalformat,
										GLsizei width, GLsizei height);
EXT_API void(EXT_ENTRY *glTexStorage3D)(GLenum target, GLsizei levels, GLenum internalformat,
										GLsizei width, GLsizei height, GLsizei depth);
EXT_API void(EXT_ENTRY *glTexStorage2DMultisample)(GLenum target, GLsizei samples,
												   GLenum internalformat, GLsizei width,
												   GLsizei height, GLboolean fixedsamplelocations);
EXT_API void(EXT_ENTRY *glTexImage2DMultisample)(GLenum target, GLsizei samples,
												 GLenum internalformat, GLsizei width,
												 GLsizei height, GLboolean fixedsamplelocations);
EXT_API void(EXT_ENTRY *glTexStorage3DMultisample)(GLenum target, GLsizei samples,
												   GLenum internalformat, GLsizei width,
												   GLsizei height, GLsizei depth,
												   GLboolean fixedsamplelocations);

EXT_API void(EXT_ENTRY *glGetProgramBinary)(GLuint program, GLsizei bufSize, GLsizei *length,
											GLenum *binaryFormat, void *binary);
EXT_API void(EXT_ENTRY *glGetProgramInterfaceiv)(GLuint program, GLenum programInterface,
												 GLenum pname, GLint *params);
EXT_API void(EXT_ENTRY *glGetProgramResourceName)(GLuint program, GLenum programInterface,
												  GLuint index, GLsizei bufSize, GLsizei *length,
												  GLchar *name);
EXT_API void(EXT_ENTRY *glGetProgramResourceiv)(GLuint program, GLenum programInterface,
												GLuint index, GLsizei propCount,
												const GLenum *props, GLsizei bufSize,
												GLsizei *length, GLint *params);
EXT_API const GLubyte *(EXT_ENTRY *glGetStringi)(GLenum name, GLuint index);

EXT_API void(EXT_ENTRY *glDrawArraysInstanced)(GLenum mode, GLint first, GLsizei count,
											   GLsizei instancecount);
EXT_API void(EXT_ENTRY *glDrawElementsInstanced)(GLenum mode, GLsizei count, GLenum type,
												 const void *indices, GLsizei instancecount);
EXT_API void(EXT_ENTRY *glDrawElementsBaseVertex)(GLenum mode, GLsizei count, GLenum type,
												  const void *indices, GLint basevertex);
EXT_API void(EXT_ENTRY *glMultiDrawArraysIndirect)(GLenum mode, const void *indirect,
												   GLsizei drawcount, GLsizei stride);
EXT_API void(EXT_ENTRY *glVertexAttribIPointer)(GLuint index, GLint size, GLenum type,
												GLsizei stride, const void *pointer);

EXT_API void(EXT_ENTRY *glBufferStorage)(GLenum target, GLsizeiptr size, const void *data,
										 GLbitfield flags);
EXT_API void(EXT_ENTRY *glInvalidateBufferSubData)(GLuint buffer, GLintptr offset,
												   GLsizeiptr length);
EXT_API void(EXT_ENTRY *glInvalidateBufferData)(GLuint buffer);
EXT_API void *(EXT_ENTRY *glMapBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length,
											GLbitfield access);
EXT_API void(EXT_ENTRY *glFlushMappedBufferRange)(GLenum target, GLintptr offset,
												  GLsizeiptr length);
EXT_API void(EXT_ENTRY *glCopyBufferSubData)(GLenum readTarget, GLenum writeTarget,
											 GLintptr readOffset, GLintptr writeOffset,
											 GLsizeiptr size);

EXT_API void(EXT_ENTRY *glGenTransformFeedbacks)(GLsizei n, GLuint *ids);
EXT_API void(EXT_ENTRY *glGenProgramPipelines)(GLsizei n, GLuint *pipelines);
EXT_API void(EXT_ENTRY *glGenSamplers)(GLsizei count, GLuint *samplers);
EXT_API void(EXT_ENTRY *glDeleteSamplers)(GLsizei count, const GLuint *samplers);
EXT_API void(EXT_ENTRY *glDeleteProgramPipelines)(GLsizei n, const GLuint *pipelines);
EXT_API void(EXT_ENTRY *glDeleteTransformFeedbacks)(GLsizei n, const GLuint *ids);

EXT_API void(EXT_ENTRY *glBindSampler)(GLuint unit, GLuint sampler);
EXT_API void(EXT_ENTRY *glBindTextures)(GLuint first, GLsizei count, const GLuint *textures);
EXT_API void(EXT_ENTRY *glBlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha);
EXT_API void(EXT_ENTRY *glBlendFuncSeparate)(GLenum sfactorRGB, GLenum dfactorRGB,
											 GLenum sfactorAlpha, GLenum dfactorAlpha);

EXT_API void(EXT_ENTRY *glVertexAttribI1i)(GLuint index, GLint x);
EXT_API void(EXT_ENTRY *glProvokingVertex)(GLenum mode);

#undef EXT_API
#undef EXT_ENTRY

#endif

#ifndef FWK_GFX_OPENGL_H_ONLY_EXTENSIONS

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/gfx_base.h"
#include "fwk/vector.h"

namespace fwk {

void testGlError(const char *);
bool installGlDebugHandler();

DEFINE_ENUM(GlVendor, intel, nvidia, amd, other);
DEFINE_ENUM(GlFeature, vertex_array_object, debug, copy_image, separate_shader_objects,
			shader_draw_parameters, shader_ballot, shader_subgroup, texture_view, texture_storage,
			texture_s3tc, texture_filter_anisotropic, timer_query);
using GlFeatures = EnumFlags<GlFeature>;

// TODO: rename to GlMax
DEFINE_ENUM(GlLimit, max_elements_indices, max_elements_vertices, max_uniform_block_size,
			max_texture_size, max_texture_3d_size, max_texture_buffer_size, max_texture_anisotropy,
			max_texture_units, max_uniform_locations, max_ssbo_bindings, max_compute_ssbo,
			max_compute_work_group_invocations, max_samples);

DEFINE_ENUM(GlDebug, not_active_uniforms);
using GlDebugFlags = EnumFlags<GlDebug>;
extern GlDebugFlags gl_debug_flags;

struct GlInfo {
	GlVendor vendor;
	GlProfile profile;

	// All extensions without the GL_ prefix
	vector<string> extensions;

	GlFeatures features;
	EnumMap<GlLimit, int> limits;

	int3 max_compute_work_group_size;
	int3 max_compute_work_groups;

	bool hasExtension(Str) const;
	bool hasFeature(GlFeature feature) const { return (bool)(features & feature); }

	string renderer;
	string version_full;
	string glsl_version_full;

	float version;
	float glsl_version;

	string toString() const;
};

void clearColor(FColor);
void clearColor(IColor);
void clearDepth(float);

extern const GlInfo *const gl_info;
}

#endif

#endif
