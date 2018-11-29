// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(VertexBaseType, int8, uint8, int16, uint16, int32, uint32, float16, float32);
DEFINE_ENUM(IndexType, uint8, uint16, uint32);

constexpr bool isIntegral(VertexBaseType type) { return type <= VertexBaseType::uint32; }

int dataSize(VertexBaseType);
int dataSize(IndexType);

DEFINE_ENUM(VertexAttribOpt, normalized, as_integer);
using VertexAttribFlags = EnumFlags<VertexAttribOpt>;

struct VertexAttrib {
	using DataType = VertexBaseType;
	using Opt = VertexAttribOpt;
	using Flags = EnumFlags<Opt>;

	constexpr VertexAttrib(DataType type, int size, int padding, Flags flags, NoAssertsTag)
		: type(type), size(size), padding(padding), flags(flags) {}
	VertexAttrib(DataType type, int size, int padding = 0, Flags flags = {});
	VertexAttrib() = default;

	int dataSize() const { return fwk::dataSize(type) * size + padding; }

	DataType type = DataType::uint8;
	unsigned char size = 1;
	unsigned char padding = 0;
	Flags flags = {};
};

namespace detail {

	template <class T>
	static constexpr bool valid_vb_type = is_one_of<T, i8, u8, i16, u16, i32, u32, float>;

	template <class T, EnableIf<valid_vb_type<T>>...> constexpr VertexBaseType vbType() {
#define CASE(type, value)                                                                          \
	if constexpr(is_same<T, type>)                                                                 \
		return VertexBaseType::value;
		CASE(i8, int8)
		CASE(u8, uint8)
		CASE(i16, int16)
		CASE(u16, uint16)
		CASE(i32, int32)
		CASE(u32, uint32)
		CASE(float, float32)
#undef CASE
		return VertexBaseType::float32;
	}

	template <VertexBaseType bt, int size, int padding, int bits>
	static constexpr VertexAttrib default_va{bt, size, padding, VertexAttribFlags{bits},
											 no_asserts_tag};

	template <class T, int size = 1> constexpr const VertexAttrib *defaultVABase() {
		if constexpr(valid_vb_type<T> && size >= 1 && size <= 4) {
			constexpr auto base_type = vbType<T>();
			constexpr auto flags =
				isIntegral(base_type) ? VertexAttribOpt::as_integer : VertexAttrib::Flags();
			return &default_va<base_type, size, 0, flags.bits>;
		}
		return nullptr;
	}

	template <class T> constexpr const VertexAttrib *defaultVA() {
		if constexpr(isVec<T>())
			return defaultVABase<fwk::VecScalar<T>, vec_size<T>>();
		if constexpr(is_same<T, IColor>) {
			return &default_va<VertexBaseType::uint8, 4, 0, flag(VertexAttribOpt::normalized).bits>;
		}

		return defaultVABase<T, 1>();
	}
}

template <class T>
static constexpr bool has_default_vertex_attrib = detail::defaultVA<T>() != nullptr;

// Default vertex attribs:
// - all standard vector types (float2, float3, int3, etc.)
// - base types
// - integer types will be treated as such (no normalization)
// - IColor (will be treated as normalized vec4)
template <class T> VertexAttrib defaultVertexAttrib() {
	static_assert(has_default_vertex_attrib<T>);
	return *detail::defaultVA<T>();
};

template <class... Args> vector<VertexAttrib> defaultVertexAttribs() {
	return {defaultVertexAttrib<Args>()...};
};

class GlVertexArray {
	GL_CLASS_DECL(GlVertexArray)
  public:
	static constexpr int max_attribs = 7;

	template <class... Args> static PVertexArray make(Args &&... args) {
		return PVertexArray(storage.make(std::forward<Args>(args)...));
	}

	void set(CSpan<PBuffer>, CSpan<VertexAttrib>);
	void set(CSpan<PBuffer>, CSpan<VertexAttrib>, PBuffer, IndexType);
	void setIndices(PBuffer, IndexType);

	// TODO: what offset means? make it type safe
	void draw(PrimitiveType, int num_elements, int element_offset = 0) const;
	void draw(PrimitiveType primitive_type) const { draw(primitive_type, size()); }
	void drawInstanced(PrimitiveType, int num_elements, int num_instances, int offset = 0);

	// TODO: offset type: Size<DrawIndirectCommand>(); But what if the user would like
	// to offset by smaller number of bytes ?
	// Size<> default converts only if can divide with modulus == 0
	void drawIndirect(PrimitiveType, PBuffer command_buffer, int num_commands = -1,
					  int offset = 0) const;

	void bind() const;
	static void unbind();

	CSpan<PBuffer> buffers() const { return {m_vertex_buffers, m_num_attribs}; }
	CSpan<VertexAttrib> attribs() const { return {m_attribs, m_num_attribs}; }

	PBuffer indexBuffer() const { return m_index_buffer; }
	IndexType indexType() const { return m_index_type; }

	int numAttribs() const { return m_num_attribs; }
	int size() const;

	// TODO: size() should return number of vertices

  private:
	void fill();
	void bindVertexBuffer(int n) const;

	PBuffer m_vertex_buffers[max_attribs];
	VertexAttrib m_attribs[max_attribs];
	PBuffer m_index_buffer;
	unsigned char m_num_attribs = 0;
	IndexType m_index_type;
	const bool m_has_vao = false;
};
}
