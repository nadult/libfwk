// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_object.h"
#include "fwk/pod_vector.h"
#include "fwk_range.h"

#include "fwk/format.h"

namespace fwk {

template <GlTypeId> class GlPtr;

// TODO: is this mapping really necessary ?
#define DEF_GL_PTR(name, id)                                                                       \
	struct name : public GlPtr<GlTypeId::id> {                                                     \
		using Super = GlPtr<GlTypeId::id>;                                                         \
		using Super::GlPtr;                                                                        \
		using Super::operator=;                                                                    \
	};

// When using GlObjects, you have to keep in mind, that:
// - when new object is created, previous object may be moved in memory
//   that's why it's better not to keep pointers or references to those
//   objects, but GlPtr;
// - both GlPtr and GlObjects have to be managed on gfx thread
//
// - GlObjects are freed when their reference count drops to 0 (No GlPtrs pointing to it exists).
template <GlTypeId type_id_> class GlStorage {
  public:
	using T = GlIdToType<type_id_>;
	static constexpr GlTypeId type_id = type_id_;

	// Big ids are really worst-case scenario
	// In their case object_id doesn't have to be equal to gl_id
	// big_id value should be tuned so that most of the OpenGL id's are smaller than that
	static constexpr int big_id =
		isOneOf(type_id, GlTypeId::buffer, GlTypeId::texture) ? 1 << 16 : 1 << 10;

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

// TODO: it's not really a pointer; choose a better name ?
// Identifies object stored in GlStorage
// Reference counted; object is destroyed when there is no GLPtrs pointing to it
template <GlTypeId type_id_> class GlPtr {
  public:
	using T = GlIdToType<type_id_>;
	static constexpr GlTypeId type_id = type_id_;

	GlPtr() : m_id(0) {}
	GlPtr(const GlPtr &rhs) : m_id(rhs.m_id) { incRefs(); }
	GlPtr(GlPtr &&rhs) : m_id(rhs.m_id) { rhs.m_id = 0; }
	~GlPtr();

	void operator=(const GlPtr &);
	void operator=(GlPtr &&);
	void swap(GlPtr &rhs) { fwk::swap(m_id, rhs.m_id); }

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

  private:
	static GlStorage<type_id_> g_storage;

	friend T;
	GlPtr(int id) : m_id(id) { incRefs(); }

	void decRefs();
	void incRefs() {
		if(m_id)
			g_storage.counters[m_id]++;
	}

	int m_id;
};

#define EXTERN_STORAGE(id) extern template GlStorage<GlTypeId::id> GlPtr<GlTypeId::id>::g_storage;
EXTERN_STORAGE(buffer)
EXTERN_STORAGE(vertex_array)
#undef EXTERN_STORAGE
}
