// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"
#include "fwk/sys/immutable_ptr.h"

namespace fwk {

DEFINE_ENUM(VertexDataType_, int8, uint8, int16, uint16, int32, uint32, float16, float32);

constexpr bool isIntegral(VertexDataType_ type) { return type <= VertexDataType_::uint32; }
int dataSize(VertexDataType_ type);

struct VertexAttrib {
	using DataType = VertexDataType_;

	VertexAttrib(DataType type, int size, int padding = 0, bool normalized = false)
		: type(type), size(size), padding(padding), normalized(normalized) {
		if(!isIntegral(type))
			PASSERT(!normalized);
		PASSERT(size >= 0 && size <= 255);
		PASSERT(padding >= 0 && padding <= 255);
	}
	VertexAttrib() = default;

	int dataSize() const { return fwk::dataSize(type) * size + padding; }

	DataType type = DataType::uint8;
	unsigned char size = 1;
	unsigned char padding = 0;
	bool normalized = false;
};

struct VertexBufferDef {
	PBuffer buffer;
	VertexAttrib attrib;
};

DEFINE_ENUM(IndexDataType, uint8, uint16, uint32);

struct IndexBufferDef {
	PBuffer buffer;
	IndexDataType data_type;
};

int dataSize(IndexDataType);

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
	void draw(PrimitiveType, int num_elements, int offset = 0) const;
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
	IndexDataType indexDataType() const { return m_index_data_type; }

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
	IndexDataType m_index_data_type;
	const bool m_has_vao = false;
};
}
