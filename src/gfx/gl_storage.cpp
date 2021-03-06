// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx/gl_storage.h"

#include "fwk/format.h"
#include "fwk/gfx/opengl.h"
#include "fwk/hash_map.h"
#include "fwk/sys/assert.h"

#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx/gl_framebuffer.h"
#include "fwk/gfx/gl_program.h"
#include "fwk/gfx/gl_query.h"
#include "fwk/gfx/gl_renderbuffer.h"
#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx/gl_texture.h"
#include "fwk/gfx/gl_vertex_array.h"

namespace fwk {

template <class T> struct Internal {
	// Only for big numbers; small are mapped directly
	HashMap<int, int> to_gl, from_gl;
	vector<uint> dummies;
	uint num_dummies = 0;

	// When GL cannot create object of given type (because extension is unavailable or something),
	// but we still need some index, then we can generate dummy id's
	uint allocDummy() {
		if(dummies.empty()) {
			num_dummies++;
			return num_dummies;
		}
		auto out = dummies.back();
		dummies.pop_back();
		return out;
	}

	void freeDummy(uint id) { dummies.emplace_back(id); }
};

template <class T> static Internal<T> s_internal;

template <class T> int GlStorage<T>::allocGL() {
	GLuint value = 0;
	PASSERT_GL_THREAD();
#define CASE(otype, func)                                                                          \
	if constexpr(is_same<T, otype>)                                                                \
		func(1, &value);

	CASE(GlBuffer, glGenBuffers)
	CASE(GlQuery, glGenQueries)
	CASE(GlProgramPipeline, glGenProgramPipelines)
	CASE(GlTransformFeedback, glGenTransformFeedbacks)
	CASE(GlSampler, glGenSamplers)
	CASE(GlTexture, glGenTextures)
	CASE(GlRenderbuffer, glGenRenderbuffers)
	CASE(GlFramebuffer, glGenFramebuffers)

#undef CASE

	if constexpr(is_same<T, GlVertexArray>) {
		if(gl_info->hasFeature(GlFeature::vertex_array_object))
			glGenVertexArrays(1, &value);
		else
			value = s_internal<T>.allocDummy();
	}
	if constexpr(is_same<T, GlProgram>)
		value = glCreateProgram();
	if constexpr(is_same<T, GlShader>)
		FATAL("Use custom function for shaders & programs");

	DASSERT(GLuint(int(value)) == value);
	return (int)value;
}

template <class T> void GlStorage<T>::freeGL(int id) {
	GLuint value = id;
	PASSERT_GL_THREAD();

#define CASE(otype, func)                                                                          \
	if constexpr(is_same<T, otype>)                                                                \
		func(1, &value);

	CASE(GlBuffer, glDeleteBuffers)
	CASE(GlQuery, glDeleteQueries)
	CASE(GlProgramPipeline, glDeleteProgramPipelines)
	CASE(GlTransformFeedback, glDeleteTransformFeedbacks)
	CASE(GlSampler, glDeleteSamplers)
	CASE(GlTexture, glDeleteTextures)
	CASE(GlRenderbuffer, glDeleteRenderbuffers)
	CASE(GlFramebuffer, glDeleteFramebuffers)

	if constexpr(is_same<T, GlVertexArray>) {
		if(gl_info->hasFeature(GlFeature::vertex_array_object))
			glDeleteVertexArrays(1, &value);
		else
			s_internal<T>.freeDummy(value);
	}
	if constexpr(is_same<T, GlShader>)
		glDeleteShader(value);
	if constexpr(is_same<T, GlProgram>)
		glDeleteProgram(value);
#undef CASE
}

template <class T> int GlStorage<T>::bigIdFromGL(int id) const {
	auto &map = s_internal<T>.from_gl;
	return map.find(id)->value;
}

template <class T> int GlStorage<T>::bigIdToGL(int id) const {
	auto &map = s_internal<T>.to_gl;
	return map.find(id)->value;
}

template <class T> void GlStorage<T>::mapBigId(int obj_id, int gl_id) {
	s_internal<T>.from_gl[gl_id] = obj_id;
	s_internal<T>.to_gl[obj_id] = gl_id;
}

template <class T> void GlStorage<T>::clearBigId(int obj_id) {
	auto &map = s_internal<T>.to_gl;
	auto it = map.find(obj_id);
	s_internal<T>.from_gl.erase(it->value);
	map.erase(it);
}

template <class T> int GlStorage<T>::allocId(int gl_id) {
	PASSERT(gl_id >= 0);

	if(gl_id >= big_id) {
		if(first_free == 0)
			resizeBuffers(max(big_id + 1024, counters.size() + 1));
		int obj_id = first_free;
		PASSERT(obj_id != 0);
		PASSERT_GT(first_free, 0);
		PASSERT_LT(first_free, counters.size());
		first_free = counters[obj_id];

		counters[obj_id] = 0;
		mapBigId(obj_id, gl_id);
		return obj_id;
	} else {
		if(gl_id >= counters.size())
			resizeBuffers(gl_id + 1);
		PASSERT(counters[gl_id] == 0);
		return gl_id;
	}
}

template <class T> void GlStorage<T>::freeId(int obj_id) {
	if(obj_id < big_id) {
		PASSERT(counters[obj_id] == 0);
	} else {
		counters[obj_id] = first_free;
		first_free = obj_id;
		clearBigId(obj_id);
	}
}

template <class T> void GlStorage<T>::resizeBuffers(int new_size) {
	int old_size = objects.size();
	new_size = vectorInsertCapacity<T>(old_size, new_size);

	PodVector<T> new_objects(new_size);

	if(objects) {
		vector<bool> is_allocated(objects.size(), true);
		for(int n = 0; n < min(big_id, objects.size()); n++)
			is_allocated[n] = counters[n] > 0;
		is_allocated[0] = false;
		int free_id = first_free;
		while(free_id) {
			is_allocated[free_id] = false;
			free_id = counters[free_id];
		}
		for(int n = 0; n < objects.size(); n++)
			if(is_allocated[n]) {
				new(&new_objects[n]) T(move(objects[n]));
				objects[n].~T();
			}
	}

	PodVector<int> new_counters(new_size);
	memcpy(new_counters.data(), counters.data(), sizeof(int) * counters.size());

	int end_fill = min(new_size, big_id);
	for(int n = old_size; n < end_fill; n++)
		new_counters[n] = 0;

	// Filling free list
	int prev_free = 0;
	int last_free = first_free;
	while(last_free != 0) {
		prev_free = last_free;
		last_free = new_counters[last_free];
	}

	int begin_list = max(big_id, old_size);
	for(int n = begin_list; n < new_size; n++) {
		if(prev_free == 0)
			first_free = n;
		else
			new_counters[prev_free] = n;
		prev_free = n;
	}
	new_counters[prev_free] = 0;

	new_objects.swap(objects);
	new_counters.unsafeSwap(counters);
}

template <class T> void GlStorage<T>::destroy(int obj_id) {
	DASSERT(counters[obj_id] == 0);
	objects[obj_id].~T();
	freeGL(toGL(obj_id));
	freeId(obj_id);
}

template <class T> GlRef<T>::~GlRef() { decRefs(); }

template <class T> void GlRef<T>::operator=(const GlRef &rhs) {
	if(&rhs == this)
		return;
	decRefs();
	m_id = rhs.m_id;
	incRefs();
}

template <class T> void GlRef<T>::operator=(GlRef &&rhs) {
	if(&rhs == this)
		return;
	decRefs();
	m_id = rhs.m_id;
	rhs.m_id = 0;
}

template <class T> void GlRef<T>::reset() {
	if(m_id) {
		decRefs();
		m_id = 0;
	}
}

template <class T> void GlRef<T>::decRefs() {
	if(m_id && --g_storage.counters[m_id] == 0)
		g_storage.destroy(m_id);
}

template <class T> GlStorage<T> GlRef<T>::g_storage;

#define INSTANTIATE(type)                                                                          \
	template class GlStorage<type>;                                                                \
	template class GlRef<type>;

INSTANTIATE(GlBuffer)
INSTANTIATE(GlVertexArray)
INSTANTIATE(GlProgram)
INSTANTIATE(GlShader)
INSTANTIATE(GlFramebuffer)
INSTANTIATE(GlRenderbuffer)
INSTANTIATE(GlTexture)
INSTANTIATE(GlQuery)

#undef INSTANTIATE
}
