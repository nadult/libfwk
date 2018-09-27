// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"

namespace fwk {

DEFINE_ENUM(MapOpt, read, write, invalidate_range, invalidate_buffer, flush_explicit, unsychronized,
			persistent, coherent);
using MapFlags = EnumFlags<MapOpt>;

DEFINE_ENUM(ImmBufferOpt, map_read, map_write, map_persistent, map_coherent, dynamic_storage,
			client_storage);
using ImmBufferFlags = EnumFlags<ImmBufferOpt>;

DEFINE_ENUM(BufferUsage, stream_draw, stream_read, stream_copy, static_draw, static_read,
			static_copy, dynamic_draw, dynamic_read, dynamic_copy);

class GlBuffer {
	GL_CLASS_DECL(GlBuffer)
  public:
	using Type = BufferType;

	static PBuffer make(Type);
	static PBuffer make(Type type, int size, BufferUsage = BufferUsage::dynamic_copy);
	static PBuffer make(Type, int size, ImmBufferFlags);

	template <class T> static PBuffer make(Type type, CSpan<T> data) {
		auto ref = make(type);
		ref->upload(data);
		return ref;
	}
	template <class TSpan, class T = SpanBase<TSpan>>
	static PBuffer make(Type type, const TSpan &span) {
		return make(type, cspan(span));
	}

	void resize(int new_size, BufferUsage = BufferUsage::dynamic_copy);
	void upload(CSpan<char>);
	void download(Span<char>) const;
	void invalidate();

	void clear(TextureFormat, int);

	template <class T> void upload(CSpan<T> data) { upload(data.template reinterpret<char>()); }
	template <class TSpan, class T = SpanBase<TSpan>> void upload(const TSpan &data) {
		upload<T>(cspan(data));
	}

	template <class T> void download(Span<T> data) const {
		download(data.template reinterpret<char>());
	}

	void copyTo(PBuffer target, int read_offset, int write_offset, int size) const;

	template <class T> vector<T> download() const { return download<T>(m_size / sizeof(T)); }

	template <class T> vector<T> download(int count) const {
		DASSERT(count >= 0 && count <= int(m_size / sizeof(T)));
		PodVector<T> out(count);
		download<T>(out);
		vector<T> vout;
		out.unsafeSwap(vout);
		return vout;
	}

	void *map(AccessMode);
	bool unmap();
	static bool unmap(Type);

	// TODO: do we really need 64-bit numbers here ?
	void *map(i64 offset, i64 size, MapFlags);
	void flushMapped(i64 offset, i64 size);

	int size() const { return m_size; }

	template <class T> T *map(AccessMode mode) { return (T *)map(mode); }
	template <class T> T *map(int first, int count, MapFlags flags) {
		return (T *)map(i64(first) * sizeof(T), i64(count) * sizeof(T), flags);
	}

	template <class T> int size() const { return size() / sizeof(T); }

	Type type() const { return m_type; }

	void bind() const;
	void unbind() const;
	static void unbind(Type);

	// TODO: size<Type>(): sizeof(Type) to jednostka

	void bindIndex(int binding_index);
	void bindIndexAs(int, BufferType);

	void validate();

  private:
	int m_size = 0;
	Type m_type;
};
}
