// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx_base.h"
#include "fwk_math.h"

namespace fwk {

class VertexArraySource {
  public:
	VertexArraySource(PVertexBuffer buffer, int offset = 0);
	VertexArraySource(const float4 &value);
	VertexArraySource(const float3 &value) : VertexArraySource(float4(value, 0.0f)) {}
	VertexArraySource(const float2 &value) : VertexArraySource(float4(value, 0.0f, 0.0f)) {}
	VertexArraySource(float value) : VertexArraySource(float4(value, 0.0f, 0.0f, 0.0f)) {}
	VertexArraySource(IColor color) : VertexArraySource(FColor(color)) {}
	VertexArraySource(FColor color) : VertexArraySource(float4(color)) {}
	VertexArraySource(VertexArraySource &&) = default;
	VertexArraySource(const VertexArraySource &) = default;

	int maxSize() const;
	PVertexBuffer buffer() const { return m_buffer; }
	const float4 &singleValue() const { return m_single_value; }
	int offset() const { return m_offset; }

  private:
	friend class VertexArray;
	PVertexBuffer m_buffer;
	float4 m_single_value;
	int m_offset;
};

class VertexArray : public immutable_base<VertexArray> {
  public:
	using Source = VertexArraySource;

	VertexArray(vector<Source>, PIndexBuffer = PIndexBuffer());
	~VertexArray();

	static immutable_ptr<VertexArray> make(vector<Source> sources,
										   PIndexBuffer index_buffer = PIndexBuffer()) {
		return make_immutable<VertexArray>(move(sources), move(index_buffer));
	}

	void operator=(const VertexArray &) = delete;
	VertexArray(const VertexArray &) = delete;

	void draw(PrimitiveType, int num_vertices, int offset = 0) const;
	void draw(PrimitiveType primitive_type) const { draw(primitive_type, size()); }

	const auto &sources() const { return m_sources; }
	PIndexBuffer indexBuffer() const { return m_index_buffer; }
	int size() const { return m_size; }

  private:
	void init();
	void bind() const;
	bool bindVertexBuffer(int n) const;
	static void unbind();

	vector<Source> m_sources;
	PIndexBuffer m_index_buffer;
	int m_size;
#if OPENGL_VERSION >= 0x30
	unsigned m_handle;
#else
	static int s_max_bind;
#endif
};
}
