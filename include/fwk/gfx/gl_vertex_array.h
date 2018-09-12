// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"
#include "fwk/sys/immutable_ptr.h"

namespace fwk {

// TODO: VertexBaseType ?
DEFINE_ENUM(VertexDataType_, int8, uint8, int16, uint16, int32, uint32, float16, float32);

// TODO: IndexType ? GlIndexType ?
DEFINE_ENUM(IndexDataType, uint8, uint16, uint32);

constexpr bool isIntegral(VertexDataType_ type) { return type <= VertexDataType_::uint32; }

int dataSize(VertexDataType_);
int dataSize(IndexDataType);

DEFINE_ENUM(VertexAttribOpt, normalized, as_integer);

struct VertexAttrib {
	using DataType = VertexDataType_;
	using Opt = VertexAttribOpt;
	using Flags = EnumFlags<Opt>;

	VertexAttrib(DataType type, int size, int padding = 0, Flags flags = {})
		: type(type), size(size), padding(padding), flags(flags) {
		if(flags & (Opt::normalized | Opt::as_integer))
			PASSERT(isIntegral(type));
		if(flags & Opt::normalized)
			PASSERT(!(flags & Opt::as_integer));
		PASSERT(size >= 0 && size <= 255);
		PASSERT(padding >= 0 && padding <= 255);
	}
	VertexAttrib() = default;

	int dataSize() const { return fwk::dataSize(type) * size + padding; }

	DataType type = DataType::uint8;
	unsigned char size = 1;
	unsigned char padding = 0;
	Flags flags = {};
};

namespace detail {

	template <class T> VertexDataType_ vertexDataType() {
#define CASE(type, value) else if constexpr(isSame<T, type>()) return VertexDataType_::value;
		if constexpr(false) {
		}
		CASE(int, int32)
		CASE(uint, uint32)
		CASE(short, int16)
		CASE(unsigned short, uint16)
		CASE(char, int8)
		CASE(unsigned char, uint8)
		CASE(float, float32)
		else static_assert("Invalid data type");
#undef CASE
	}

	template <class T> struct DefaultVertexAttrib {
		static VertexAttrib make() { return {vertexDataType<T>(), 1}; }
	};
	template <class T> struct DefaultVertexAttrib<vec2<T>> {
		static VertexAttrib make() { return {vertexDataType<T>(), 2}; }
	};
	template <class T> struct DefaultVertexAttrib<vec3<T>> {
		static VertexAttrib make() { return {vertexDataType<T>(), 3}; }
	};
	template <class T> struct DefaultVertexAttrib<vec4<T>> {
		static VertexAttrib make() { return {vertexDataType<T>(), 4}; }
	};
	template <> struct DefaultVertexAttrib<IColor> {
		static VertexAttrib make() {
			return {VertexDataType_::uint8, 4, 0, VertexAttribOpt::normalized};
		}
	};
}

template <class T> VertexAttrib defaultVertexAttrib() {
	return detail::DefaultVertexAttrib<T>::make();
};

template <class... Args> vector<VertexAttrib> defaultVertexAttribs() {
	return {detail::DefaultVertexAttrib<Args>::make()...};
};

struct VertexBufferDef {
	PBuffer buffer;
	VertexAttrib attrib;
};

struct IndexBufferDef {
	PBuffer buffer;
	IndexDataType data_type;
};

class GlVertexArray {
  public:
	static constexpr int max_attribs = 7;

	int id() const { return m_has_vao ? storage.glId(this) : 0; }

	GlVertexArray();
	~GlVertexArray();

	void set(CSpan<PBuffer>, CSpan<VertexAttrib>);
	void set(CSpan<PBuffer>, CSpan<VertexAttrib>, PBuffer, IndexDataType);

	void operator=(const GlVertexArray &) = delete;
	GlVertexArray(const GlVertexArray &) = delete;

	template <class... Args> static PVertexArray2 make(Args &&... args) {
		return PVertexArray2(storage.make(std::forward<Args>(args)...));
	}

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
	IndexDataType indexType() const { return m_index_type; }

	int numAttribs() const { return m_num_attribs; }
	int size() const;

	// TODO: size() should return number of vertices

  private:
	static constexpr auto &storage = PVertexArray2::g_storage;

	void fill();
	void bindVertexBuffer(int n) const;

	PBuffer m_vertex_buffers[max_attribs];
	VertexAttrib m_attribs[max_attribs];
	PBuffer m_index_buffer;
	unsigned char m_num_attribs = 0;
	IndexDataType m_index_type;
	const bool m_has_vao = false;
};
}
