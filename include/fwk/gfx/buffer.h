// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx_base.h"
#include "fwk/sys/immutable_ptr.h"
#include "fwk_range.h"

namespace fwk {

DEFINE_ENUM(MapBit, read, write, invalidate_range, invalidate_buffer, flush_explicit, unsychronized,
			persistent, coherent);
using MapFlags = EnumFlags<MapBit>;

// TODO: better name ?
// GfxBuffer ? TextureFormat similarly: GfxFormat ?
class Buffer : public immutable_base<Buffer> {
  public:
	using Type = BufferType;

	template <class T> Buffer(CSpan<T> data) : Buffer() { upload(data); }
	Buffer(int size) : Buffer() { resize(size); }

	Buffer();
	~Buffer();

	void operator=(const Buffer &) = delete;
	Buffer(const Buffer &) = delete;

	void resize(int new_size);
	void upload(CSpan<char>);
	void download(Span<char>) const;

	void clear(TextureFormat, int);

	template <class T> void upload(CSpan<T> data) { upload(data.template reinterpret<char>()); }

	template <class T> void download(Span<T> data) const {
		download(data.template reinterpret<char>());
	}

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

	void *map(i64 offset, i64 size, MapFlags);
	void flushMapped(i64 offset, i64 size);

	int size() const { return m_size; }
	int id() const { return m_handle; }
	Type type() const { return m_type; }

	void bind() const;
	void unbind() const;
	static void unbind(Type);

	void bindIndex(int binding_index);

  private:
	unsigned m_handle;
	int m_size;
	Type m_type;
};
}
