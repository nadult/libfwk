// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"

namespace fwk {

DEFINE_ENUM(GlTypeId, buffer, shader, program, vertex_array, query, program_pipeline,
			transform_feedback, sampler, texture, renderbuffer, framebuffer);

// TODO: consistent naming

class GlBuffer;
class GlShader;
class GlProgram;
class GlVertexArray;
class GlQuery;
class GlProgramPipeline;
class GlTransformFeedback;
class GlSampler;
class GlTexture;
class GlRenderbuffer;
class GlFramebuffer;

// PBuffer -> GlBuffer
// VertexBuffer(PBuffer)
// IndexBuffer (PBuffer)

namespace detail {
	template <GlTypeId> struct GlIdToType { using Type = void; };
	template <class> struct GlTypeToId {};

#define CASE(id, type)                                                                             \
	template <> struct GlIdToType<GlTypeId::id> { using Type = type; };                            \
	template <> struct GlTypeToId<type> { static constexpr GlTypeId value = GlTypeId::id; };

	CASE(buffer, GlBuffer)
	CASE(shader, GlShader)
	CASE(program, GlProgram)
	CASE(vertex_array, GlVertexArray)
	CASE(query, GlQuery)
	CASE(transform_feedback, GlTransformFeedback)
	CASE(sampler, GlSampler)
	CASE(texture, GlTexture)
	CASE(renderbuffer, GlRenderbuffer)
	CASE(framebuffer, GlFramebuffer)

#undef CASE
}

template <class T> static constexpr GlTypeId gfx_type_id = detail::GlTypeToId<T>::value;
template <GlTypeId id> using GlIdToType = typename detail::GlIdToType<id>::Type;
}
