// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/pod_vector.h"
#include "fwk_range.h"

namespace fwk {

// When using GlObjects, you have to keep in mind, that:
// - when new object is created, previous object may be moved in memory
//   that's why it's better not to keep pointers or references to those objects, but GlRef
// - both GlRef and GlObjects have to be managed on gfx thread
//
// - GlObjects are freed when their reference count drops to 0 (No GlRefs pointing to it exists).
template <class T> class GlStorage {
  public:
	// Big ids are really worst-case scenario
	// In their case object_id doesn't have to be equal to gl_id
	// big_id value should be tuned so that most of the OpenGL id's are smaller than that
	static constexpr int big_id = IsOneOf<T, GlBuffer, GlTexture>::value ? 1 << 16 : 1 << 10;

	bool contains(const T *ptr) const { return ptr >= objects.data() && ptr + 1 <= objects.end(); }
	int objectId(const T *ptr) const { return int(ptr - objects.data()); }
	int glId(const T *ptr) const { return toGL(objectId(ptr)); }

	template <class... Args> int make(Args &&... args) {
		int gl_id = allocGL();
		int obj_id = allocId(gl_id);
		new(&objects[obj_id]) T(std::forward<Args>(args)...);
		return obj_id;
	}

	// TODO: uints for gl_id ?

	int fromGL(int gl_id) const { return gl_id < big_id ? gl_id : bigIdFromGL(gl_id); }
	int toGL(int obj_id) const { return obj_id < big_id ? obj_id : bigIdToGL(obj_id); }

	int allocGL();
	void freeGL(int);

	int allocId(int gl_id);
	void freeId(int obj_id);
	void resizeBuffers(int new_size);
	void destroy(int obj_id);

	int bigIdToGL(int obj_id) const;
	int bigIdFromGL(int gl_id) const;
	void mapBigId(int obj_id, int gl_id);
	void clearBigId(int obj_id);

	// - for allocated objects: stores ref counts
	// - for unallocated objects < big_id:  0
	// - for unallocated objects >= big_id: free list node
	vector<int> counters;
	PodVector<T> objects;
	int first_free = 0;
};

// Identifies object stored in GlStorage
// Reference counted; object is destroyed when there is no GlRefs pointing to it
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
#undef EXTERN_STORAGE
}
