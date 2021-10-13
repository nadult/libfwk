// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/pod_vector.h"

namespace fwk {

// When using GlObjects, you have to keep in mind, that:
// - when new object is created, previous object may be moved in memory
//   that's why it's better not to keep pointers or references to those objects, but GlRef
// - both GlRef and GlObjects have to be managed on gfx thread
// - GlObjects are freed when their reference count drops to 0 (No GlRefs pointing to it exists).
// - All GlObjects have to be freed before OpenGL device is destroyed
template <class T> class GlStorage {
  public:
	// Big ids are really worst-case scenario
	// In their case object_id doesn't have to be equal to gl_id
	// big_id value should be tuned so that most of the OpenGL id's are smaller than that
	static constexpr int big_id = is_one_of<T, GlBuffer, GlTexture> ? 1 << 16 : 1 << 10;

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

#define GL_CLASS_DECL(name)                                                                        \
  private:                                                                                         \
	name();                                                                                        \
	~name();                                                                                       \
	name(const name &) = delete;                                                                   \
	name(name &&);                                                                                 \
	void operator=(const name &) = delete;                                                         \
	static constexpr auto &storage = GlRef<name>::g_storage;                                       \
	friend GlStorage<name>;                                                                        \
                                                                                                   \
  public:                                                                                          \
	using Ref = GlRef<name>;                                                                       \
	int id() const { return storage.glId(this); }

#define GL_CLASS_IMPL(name)                                                                        \
	name::name() = default;                                                                        \
	name::~name() = default;                                                                       \
	name::name(name &&) = default;
}
