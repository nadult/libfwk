// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/sys/immutable_ptr.h"
#include "fwk_range.h"

#include "fwk/pod_vector.h"

namespace fwk {

class ShaderStorage : public immutable_base<ShaderStorage> {
  public:
	template <class T> ShaderStorage(CSpan<T> data) : ShaderStorage() { upload(data); }
	ShaderStorage(int size) : ShaderStorage() { resize(size); }

	ShaderStorage();
	~ShaderStorage();

	void operator=(const ShaderStorage &) = delete;
	ShaderStorage(const ShaderStorage &) = delete;

	void resize(int new_size);
	void upload(CSpan<char>);
	void download(Span<char>) const;

	void bind(int binding_index);

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

	int size() const { return m_size; }
	int id() const { return m_handle; }

  private:
	unsigned m_handle;
	int m_size;
};
}
