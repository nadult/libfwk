// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"
#include "fwk/span.h"

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

	void recreate(int new_size, BufferUsage = BufferUsage::dynamic_copy);

	// Increases size, using vectorInsertCapacity(), so it may allocate
	// more memory than necessary; old data will be copied to newly created buffer
	static void upsize(PBuffer &, int minimum_count, int type_size = 1);
	template <class T> static void upsize(PBuffer &buf, int minimum_count) {
		upsize(buf, minimum_count, sizeof(T));
	}

	void upload(CSpan<char>);
	void download(Span<char>, i64 offset = 0) const;
	void invalidate();

	void clear(GlFormat, int value);
	void clear(GlFormat, int value, int offset, int size);

	template <class T> void upload(CSpan<T> data) { upload(data.template reinterpret<char>()); }
	template <class TSpan, class T = SpanBase<TSpan>> void upload(const TSpan &data) {
		upload<T>(cspan(data));
	}

	template <class T> void download(Span<T> data, i64 offset) const {
		download(data.template reinterpret<char>(), offset * sizeof(T));
	}

	void copyTo(PBuffer target, int read_offset, int write_offset, int size) const;

	template <class T> vector<T> download() const { return download<T>(m_size / sizeof(T)); }

	template <class T> vector<T> download(i64 count, i64 offset = 0) const {
		DASSERT(offset >= 0 && count >= 0);
		DASSERT(count + offset <= i64(m_size / sizeof(T)));
		PodVector<T> out(count);
		download<T>(out, offset);
		vector<T> vout;
		out.unsafeSwap(vout);
		return vout;
	}

	void *map(AccessMode);
	bool unmap();
	static bool unmap(Type);

	// Will also bind buffer
	bool isMapped() const;

	// TODO: do we really need 64-bit numbers here ?
	void *map(i64 offset, i64 size, MapFlags);
	void flushMapped(i64 offset, i64 size);

	int size() const { return m_size; }
	int usedMemory() const { return m_size; }

	template <class T> Span<T> map(AccessMode mode) {
		return span((T *)map(mode), m_size / sizeof(T));
	}
	template <class T> Span<T> map(int first, int count, MapFlags flags) {
		return span((T *)map(i64(first) * sizeof(T), i64(count) * sizeof(T), flags), count);
	}
	template <class T> void flushMapped(int first, int count) {
		flushMapped(i64(first) * sizeof(T), i64(count) * sizeof(T));
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
	Maybe<BufferUsage> m_usage;
	ImmBufferFlags m_imm_flags;
};
}
