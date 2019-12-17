// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/math_base.h"
#include "fwk/sys/expected.h"

namespace fwk {

DEFINE_ENUM(ProgramBindingType, shader_storage, uniform_block, atomic_counter, transform_feedback);

// Interface to OpenGL program object
// Setting uniform values on older versions (before 4.1) will also bind current program
// FWK_CHECK_OPENGL allows checking if all uniforms are set
class GlProgram {
	GL_CLASS_DECL(GlProgram)
  public:
	static Ex<PProgram> make(PShader compute);
	static Ex<PProgram> make(PShader vertex, PShader fragment, CSpan<string> location_names = {});
	static Ex<PProgram> make(PShader vertex, PShader geom, PShader fragment,
							 CSpan<string> location_names = {});

	// Source loaded from file is added at the end of sources list
	static Ex<PProgram> load(ZStr vsh_file_name, ZStr fsh_file_name, vector<string> sources = {},
							 CSpan<string> location_names = {});

	vector<Pair<string, int>> getBindings(ProgramBindingType) const;
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

	UniformSetter operator[](ZStr name);
	UniformSetter operator[](int location) { return {id(), location}; }

	void use();
	static void unbind();

	u64 hash() const { return m_hash; }
	ZStr name() const { return m_name; }

  private:
	Ex<void> set(CSpan<PShader>, CSpan<string> loc_names);
	void loadUniformInfo();
	static void setUniformInitialized(int, int);

	vector<UniformInfo> m_uniforms;
	u64 m_hash;
	string m_name;
};
}
