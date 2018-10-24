// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(ProgramBindingType, shader_storage, uniform_block, atomic_counter, transform_feedback);

// Interface to OpenGL program object
// Setting uniform values on older versions (before 4.1) will also bind current program
// FWK_CHECK_OPENGL allows checking if all uniforms are set
class GlProgram {
	GL_CLASS_DECL(GlProgram)
  public:
	static PProgram make(PShader compute);
	static PProgram make(PShader vertex, PShader fragment, CSpan<string> location_names = {});
	static PProgram make(PShader vertex, PShader geom, PShader fragment,
						 CSpan<string> location_names = {});
	static PProgram make(const string &vsh_file_name, const string &fsh_file_name,
						 const string &predefined_macros, CSpan<string> location_names = {});

	vector<pair<string, int>> getBindings(ProgramBindingType) const;
	vector<char> getBinary() const;
	string getInfo() const;

	struct UniformInfo {
		string name;
		int gl_type; // TODO: enum type
		int size;
		int location;
	};

	CSpan<UniformInfo> uniforms() const { return m_uniforms; }
	Maybe<int> findUniform(ZStr) const;

	int location(ZStr name) const {
		if(auto uid = findUniform(name))
			return m_uniforms[*uid].location;
		return -1;
	}

	struct UniformSetter {
		void operator=(CSpan<float>);
		void operator=(CSpan<float2>);
		void operator=(CSpan<float3>);
		void operator=(CSpan<float4>);
		void operator=(CSpan<int>);
		void operator=(CSpan<int2>);
		void operator=(CSpan<int3>);
		void operator=(CSpan<int4>);
		void operator=(CSpan<uint>);
		void operator=(CSpan<Matrix4>);

		void operator=(float);
		void operator=(int);
		void operator=(uint);
		void operator=(const int2 &);
		void operator=(const int3 &);
		void operator=(const int4 &);
		void operator=(const float2 &);
		void operator=(const float3 &);
		void operator=(const float4 &);
		void operator=(const Matrix4 &);

		bool valid() const { return location != -1; }

		int program_id;
		int location;
	};

	// Uniform must be present & active
	UniformSetter operator[](ZStr name) {
		int loc = location(name);
		if(loc == -1) // TODO: debug check?
			uniformNotFound(name);
		return {id(), loc};
	}
	UniformSetter operator[](int location) {
		PASSERT(location >= 0);
		return {id(), location};
	}

	// Uniform doesn't have to be active
	UniformSetter operator()(ZStr name) { return {id(), location(name)}; }
	UniformSetter operator()(int location) { return {id(), location}; }

	void use();
	static void unbind();

	// TODO: this is not very useful, because some uniforms can be set from inside of shader
#ifdef FWK_CHECK_OPENGL
	static void checkUniformsInitialized();
#else
	static void checkUniformsInitialized() {}
#endif

  private:
	void set(CSpan<PShader>, CSpan<string> loc_names);
	void loadUniformInfo();
	[[noreturn]] void uniformNotFound(ZStr) const;
	static void setUniformInitialized(int, int);

	vector<UniformInfo> m_uniforms;
	IF_GL_CHECKS(vector<bool> m_init_map; int m_uniforms_to_init = 0;)
};
}
