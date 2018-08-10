// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_storage.h"

#include "fwk/format.h"
#include "fwk/gfx/opengl.h"
#include "fwk/hash_map.h"

namespace fwk {

template <GlTypeId type_id> struct Internal {
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

template <GlTypeId type_id> static Internal<type_id> s_internal;

template <GlTypeId type_id> int GlStorage<type_id>::allocGL() {
	GLuint value;
#define CASE(otype, func)                                                                          \
	if constexpr(type_id == GlTypeId::otype)                                                       \
		func(1, &value);

	CASE(buffer, glGenBuffers)
	CASE(query, glGenQueries)
	CASE(program_pipeline, glGenProgramPipelines)
	CASE(transform_feedback, glGenTransformFeedbacks)
	CASE(sampler, glGenSamplers)
	CASE(texture, glGenTextures)
	CASE(renderbuffer, glGenRenderbuffers)
	CASE(framebuffer, glGenFramebuffers)

	if constexpr(type_id == GlTypeId::vertex_array) {
		if(opengl_info->hasFeature(OpenglFeature::vertex_array_object))
			glGenVertexArrays(1, &value);
		else
			value = s_internal<type_id>.allocDummy();
	}

	if constexpr(isOneOf(type_id, GlTypeId::shader, GlTypeId::program))
		FATAL("Use custom function for shaders & programs");
#undef CASE

	DASSERT(GLuint(int(value)) == value);
	return (int)value;
}

template <GlTypeId type_id> void GlStorage<type_id>::freeGL(int id) {
	GLuint value = id;

#define CASE(otype, func)                                                                          \
	if constexpr(type_id == GlTypeId::otype)                                                       \
		func(1, &value);

	CASE(buffer, glDeleteBuffers)
	CASE(query, glDeleteQueries)
	CASE(program_pipeline, glDeleteProgramPipelines)
	CASE(transform_feedback, glDeleteTransformFeedbacks)
	CASE(sampler, glDeleteSamplers)
	CASE(texture, glDeleteTextures)
	CASE(renderbuffer, glDeleteRenderbuffers)
	CASE(framebuffer, glDeleteFramebuffers)

	if constexpr(type_id == GlTypeId::vertex_array) {
		if(opengl_info->hasFeature(OpenglFeature::vertex_array_object))
			glDeleteVertexArrays(1, &value);
		else
			s_internal<type_id>.freeDummy(value);
	}

	if constexpr(type_id == GlTypeId::shader)
		glDeleteShader(value);
	if constexpr(type_id == GlTypeId::program)
		glDeleteProgram(value);
#undef CASE
}

template <GlTypeId type_id> int GlStorage<type_id>::bigIdFromGL(int id) const {
	auto &map = s_internal<type_id>.from_gl;
	auto it = map.find(id);
	PASSERT(it != map.end());
	return it->second;
}

template <GlTypeId type_id> int GlStorage<type_id>::bigIdToGL(int id) const {
	auto &map = s_internal<type_id>.to_gl;
	auto it = map.find(id);
	PASSERT(it != map.end());
	return it->second;
}

template <GlTypeId type_id> void GlStorage<type_id>::mapBigId(int obj_id, int gl_id) {
	s_internal<type_id>.from_gl[gl_id] = obj_id;
	s_internal<type_id>.to_gl[obj_id] = gl_id;
}

template <GlTypeId type_id> void GlStorage<type_id>::clearBigId(int obj_id) {
	auto &map = s_internal<type_id>.to_gl;
	auto it = map.find(obj_id);
	PASSERT(it != map.end());
	s_internal<type_id>.from_gl.erase(it->second);
	map.erase(it);
}

template <GlTypeId type_id> int GlStorage<type_id>::allocId(int gl_id) {
	if(gl_id >= big_id) {
		if(first_free == 0)
			resizeBuffers(max(big_id + 1024, counters.size() + 1));
		int obj_id = first_free;
		PASSERT(obj_id != 0);
		first_free = counters[obj_id];
		counters[obj_id] = 0;
		return obj_id;
	} else {
		if(gl_id >= counters.size())
			resizeBuffers(gl_id + 1);
		PASSERT(counters[gl_id] == 0);
		return gl_id;
	}
}

template <GlTypeId type_id> void GlStorage<type_id>::freeId(int obj_id) {
	if(obj_id < big_id) {
		PASSERT(counters[obj_id] == 0);
	} else {
		counters[obj_id] = first_free;
		first_free = obj_id;
		clearBigId(obj_id);
	}
}

template <GlTypeId type_id> void GlStorage<type_id>::resizeBuffers(int new_size) {
	int old_size = objects.size();
	new_size = BaseVector::insertCapacity(old_size, sizeof(T), new_size);

	PodVector<int> new_counters(new_size);
	PodVector<T> new_objects(new_size);

	// TODO: copy (in a normal way) only objects which exist
	memcpy(new_counters.data(), counters.data(), sizeof(int) * counters.size());
	memcpy(new_objects.data(), objects.data(), sizeof(T) * objects.size());

	int end_fill = min(new_size, big_id);
	for(int n = old_size; n < end_fill; n++)
		new_counters[n] = 0;

	// Filling free list
	int prev_free = 0;
	int last_free = first_free;
	while(last_free != 0) {
		prev_free = last_free;
		last_free = counters[last_free];
	}

	int begin_list = max(big_id, old_size);
	for(int n = begin_list; n < new_size; n++) {
		if(prev_free == 0)
			first_free = n;
		else
			counters[prev_free] = n;
		prev_free = n;
	}

	new_objects.swap(objects);
	new_counters.unsafeSwap(counters);
}

template <GlTypeId type_id> void GlStorage<type_id>::destroy(int obj_id) {
	DASSERT(counters[obj_id] == 0);
	objects[obj_id].~T();
	freeGL(toGL(obj_id));
	freeId(obj_id);
}

template <GlTypeId type_id> GlStorage<type_id> GlPtr<type_id>::g_storage;
}
