#pragma once

#include "fwk/gfx/gl_storage.h"

namespace fwk {

// Identifies object stored in GlStorage
// Reference counted; object is destroyed when there is no GlRefs pointing to it
// Objects are stored in a vector (they may move when new object is created)
template <class T> class GlRef {
  public:
	GlRef() : m_id(0) {}
	GlRef(const GlRef &rhs) : m_id(rhs.m_id) { incRefs(); }
	GlRef(GlRef &&rhs) : m_id(rhs.m_id) { rhs.m_id = 0; }
	~GlRef();

	void operator=(const GlRef &);
	void operator=(GlRef &&);
	void swap(GlRef &rhs) { fwk::swap(m_id, rhs.m_id); }

	T &operator*() const {
		PASSERT(m_id);
		return g_storage.objects[m_id];
	}
	T *operator->() const {
		PASSERT(m_id);
		return &g_storage.objects[m_id];
	}

	int id() const { return g_storage.toGL(m_id); }
	explicit operator bool() const { return m_id != 0; }

	int refCount() const { return g_storage.counters[m_id]; }
	void reset();

	template <class... Args> void emplace(Args &&... args) {
		*this = T::make(std::forward<Args>(args)...);
	}

	bool operator==(const GlRef &rhs) const { return m_id == rhs.m_id; }
	bool operator<(const GlRef &rhs) const { return m_id < rhs.m_id; }

  private:
	static GlStorage<T> g_storage;
	// TODO: direct pointers to objects  & counters data?
	// all the rest doesn't really have to be exposed

	friend T;
	GlRef(int id) : m_id(id) { incRefs(); }

	void decRefs();
	void incRefs() {
		if(m_id)
			g_storage.counters[m_id]++;
	}

	int m_id;
};

#define EXTERN_STORAGE(type) extern template GlStorage<type> GlRef<type>::g_storage;
EXTERN_STORAGE(GlBuffer)
EXTERN_STORAGE(GlVertexArray)
EXTERN_STORAGE(GlProgram)
EXTERN_STORAGE(GlShader)
#undef EXTERN_STORAGE
}
