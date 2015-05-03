/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_collada.h"

namespace fwk {
namespace collada {

	DEFINE_ENUM(Semantic, "VERTEX", "NORMAL", "COLOR", "TEXCOORD", "TEXTANGENT", "TEXBINORMAL",

				"WEIGHT", "JOINT", "INV_BIND_MATRIX");

	DEFINE_ENUM(SamplerSemantic, "INPUT", "OUTPUT", "INTERPOLATION", "IN_TANGENT", "OUT_TANGENT");

	DEFINE_ENUM(SourceArrayType, "IDREF_array", "Name_array", "bool_array", "float_array",
				"int_array");

	template <class T, class Func> void parseValues(XMLNode node, T *out, int count, Func func) {
		DASSERT(count >= 0);
		const char *input = node.value();

		int parsed_count = 0;
		while(input) {
			while(*input == ' ')
				input++;
			if(*input == 0)
				break;
			T value = (T)func(input);

			if(parsed_count < count)
				out[parsed_count] = value;
			parsed_count++;
			input = strchr(input, ' ');
		}

		if(count != parsed_count)
			THROW("Parsed %d values, expected %d (node: '%s')", parsed_count, count, node.name());
	}

	void parseValues(XMLNode node, int *out, int count) { parseValues(node, out, count, atoi); }
	void parseValues(XMLNode node, float *out, int count) { parseValues(node, out, count, atof); }
	void parseValues(XMLNode node, bool *out, int count) {
		parseValues<bool>(node, out, count,
						  [](const char *v) { return strcmp(v, "true") == 0 ? true : false; });
	}

	template <class T, class TBase> const T Source::get(int idx) const {
		T out;
		DASSERT(idx >= 0 && idx < m_count);
		int offset = m_offset + idx * m_stride;
		memcpy(&out, m_array.data() + offset * sizeof(TBase), sizeof(T));
		return out;
	}

	float Source::toFloat(int idx) const {
		DASSERT(m_type == type_float);
		return get<float, float>(idx);
	}

	float2 Source::toFloat2(int idx) const {
		DASSERT(m_type == type_float2);
		return get<float2, float>(idx);
	}

	float3 Source::toFloat3(int idx) const {
		DASSERT(m_type == type_float3);
		return get<float3, float>(idx);
	}

	float4 Source::toFloat4(int idx) const {
		DASSERT(m_type == type_float4);
		return get<float4, float>(idx);
	}

	Matrix4 Source::toMatrix(int idx) const {
		DASSERT(m_type == type_matrix);
		return transpose(get<Matrix4, float>(idx));
	}

	StringRef Source::toString(int idx) const {
		DASSERT(m_type == type_name);
		DASSERT(idx >= 0 && idx < m_count);
		return m_string_array[m_offset + idx * m_stride];
	}

	Source::Source(XMLNode node) : m_stride(1), m_offset(0), m_count(0), m_type(type_unknown) {
		DASSERT(node && StringRef(node.name()) == "source");
		m_id = node.attrib("id");

		XMLNode sub_node = node.child();
		if(StringRef(sub_node.name()) == "asset") // TODO: fixme
			sub_node = sub_node.sibling();

		m_array_type = SourceArrayType::fromString(sub_node.name());
		m_array_count = sub_node.attrib<int>("count");
		StringRef array_id = sub_node.attrib("id");

		if(m_array_type == SourceArrayType::idref_array ||
		   m_array_type == SourceArrayType::name_array) {
			m_string_array.resize(m_array_count);
			m_string_array = xml_conversions::fromString<vector<string>>(sub_node.value());
		} else if(m_array_type == SourceArrayType::float_array) {
			m_array.resize(sizeof(float) * m_array_count);
			parseValues(sub_node, (float *)m_array.data(), m_array_count);
		} else if(m_array_type == SourceArrayType::int_array) {
			m_array.resize(sizeof(int) * m_array_count);
			parseValues(sub_node, (int *)m_array.data(), m_array_count);
		} else if(m_array_type == SourceArrayType::bool_array) {
			m_array.resize(sizeof(bool) * m_array_count);
			parseValues(sub_node, (bool *)m_array.data(), m_array_count);
		}

		XMLNode tech_node = sub_node.sibling("technique_common");
		ASSERT(tech_node);
		XMLNode accessor = tech_node.child("accessor");
		ASSERT(accessor);

		m_stride = accessor.hasAttrib("stride") ? accessor.attrib<int>("stride") : 1;
		m_offset = accessor.hasAttrib("offset") ? accessor.attrib<int>("offset") : 0;
		m_count =
			accessor.hasAttrib("count") ? accessor.attrib<int>("count") : m_array_count / m_stride;
		ASSERT(m_count * m_stride + m_offset <= m_array_count);

		const char *source_id = accessor.attrib("source");
		ASSERT(source_id[0] == '#' && StringRef(source_id + 1) == array_id);

		vector<StringRef> param_types;
		XMLNode param_node = accessor.child("param");
		while(param_node) {
			param_types.push_back(param_node.attrib("type"));
			param_node.next();
		}

		if(m_array_type == SourceArrayType::float_array) {
			int simple_count = 0;
			for(int n = 0; n < (int)param_types.size(); n++)
				if(param_types[n] == "float")
					simple_count++;

			if(simple_count == 0 && param_types.size() == 1) {
				ASSERT(param_types.front() == "float4x4");
				m_type = type_matrix;
			} else {
				ASSERT(simple_count == (int)param_types.size());
				ASSERT(simple_count <= 4);
				m_type = simple_count == 1 ? type_float : simple_count == 2
															  ? type_float2
															  : simple_count == 3 ? type_float3
																				  : type_float4;
			}
		} else if(m_array_type == SourceArrayType::name_array ||
				  m_array_type == SourceArrayType::idref_array) {
			ASSERT(param_types.size() == 1);
			ASSERT(param_types.front() == "name");
			m_type = type_name;
		} else
			ASSERT(0 && "array_type not supported (TODO)");
	}

	int Triangles::attribIndex(Semantic::Type sem, int idx) const {
		DASSERT(idx >= 0 && idx < m_vertex_count);
		DASSERT(hasAttrib(sem));
		return m_indices[idx * m_stride + m_offsets[sem]];
	}

	Triangles::Triangles() : m_parent(nullptr), m_vertex_count(0) {
		for(int n = 0; n < Semantic::count; n++)
			m_sources[n] = nullptr;
	}

	Triangles::Triangles(Node *parent, XMLNode node) : m_parent(parent) {
		DASSERT(parent);
		DASSERT(node && StringRef(node.name()) == "triangles");

		m_material_name = node.hasAttrib("material") ? node.attrib("material") : "";
		m_vertex_count = node.attrib<int>("count") * 3;
		for(int n = 0; n < Semantic::count; n++)
			m_sources[n] = nullptr;
		m_stride = 0;

		XMLNode input_node = node.child("input");
		while(input_node) {
			Semantic::Type sem_type = Semantic::fromString(input_node.attrib("semantic"));
			m_sources[sem_type] = parent->findSource(input_node.attrib("source"));
			ASSERT(m_sources[sem_type]);

			int offset = input_node.attrib<int>("offset");
			m_offsets[sem_type] = offset;
			m_stride = max(m_stride, offset + 1);
			input_node.next();
		}

		ASSERT(m_stride != 0);

		XMLNode indices_node = node.child("p");
		ASSERT(indices_node);

		m_indices.resize(m_vertex_count * m_stride);
		parseValues(indices_node, m_indices.data(), m_indices.size());
	}

	const Source *Node::findSource(const char *id) const {
		ASSERT(id && id[0] == '#');
		for(int n = 0; n < (int)m_sources.size(); n++)
			if(m_sources[n].id() == id + 1)
				return &m_sources[n];

		return nullptr;
	}

	Node::Node(Node *parent, XMLNode node) : m_node(node), m_parent(parent) {
		DASSERT(node);
		m_id = node.hasAttrib("id") ? node.attrib("id") : "";
		parseSources(node);
	}
	void Node::parseSources(XMLNode node) {
		XMLNode source_node = node.child("source");
		while(source_node) {
			m_sources.push_back(Source(source_node));
			source_node.next();
		}
	}

	Root::Root(const XMLDocument &doc) : Node(nullptr, doc.child("COLLADA")) {
		ASSERT(m_node);

		m_up_axis = 1;
		XMLNode asset = m_node.child("asset");
		if(asset)
			if(XMLNode up_axis_node = asset.child("up_axis")) {
				StringRef up_text = up_axis_node.value();
				if(up_text == "X_UP")
					THROW("X_UP in up_axis not supported");
				m_up_axis =
					up_text == "Z_UP" ? 2 : up_text == "Y_UP" ? 1 : up_text == "X_UP" ? 0 : -1;
				ASSERT(m_up_axis != -1);
			}

		XMLNode lib_geometries = m_node.child("library_geometries");
		XMLNode lib_controllers = m_node.child("library_controllers");
		XMLNode lib_animations = m_node.child("library_animations");
		XMLNode lib_visual_scenes = m_node.child("library_visual_scenes");

		if(lib_geometries) {
			XMLNode geometry_node = lib_geometries.child("geometry");
			while(geometry_node) {
				m_meshes.push_back(new Mesh(this, geometry_node));
				geometry_node.next();
			}
		}
		if(lib_controllers) {
			XMLNode skin_node = lib_controllers.child("controller");
			while(skin_node) {
				m_skins.push_back(new Skin(this, skin_node));
				skin_node.next();
			}
		}
		if(lib_animations) {
			XMLNode anim_node = lib_animations.child("animation");
			while(anim_node) {
				m_anims.push_back(new Animation(this, anim_node));
				anim_node.next();
			}
		}
		if(lib_visual_scenes) {
			XMLNode visual_scene = lib_visual_scenes.child("visual_scene");
			while(visual_scene) {
				XMLNode node = visual_scene.child("node");

				while(node) {
					m_root_joints.push_back(new SceneNode(this, node));
					node.next();
				}
				visual_scene.next();
			}
		}
	}

	void Root::fixUpAxis(Matrix4 &mat) const {
		if(upAxis() == 2) {
			swap(mat[1], mat[2]);
			mat[0] *= -1.0f;
			mat = transpose(mat);
			mat[0] *= -1.0f;
			swap(mat[1], mat[2]);
			mat = transpose(mat);
		}
	}

	void Root::fixUpAxis(float3 &vec) const {
		if(upAxis() == 2) {
			swap(vec[1], vec[2]);
			vec[0] = -vec[0];
		}
	}

	void Root::printInfo() const {
		printf("Meshes: %d\n", meshCount());
		printf("Skins: %d\n", skinCount());
		printf("Animations: %d\n", animCount());
		printf("Root Joints: %d\n", sceneNodeCount());
	}

	Mesh::Mesh(Node *parent, XMLNode node) : Node(parent, node), m_position_source(nullptr) {
		node = node.child("mesh");
		ASSERT(node);
		parseSources(node);

		XMLNode tris_node = node.child("triangles");
		ASSERT(tris_node);

		XMLNode verts_node = node.child("vertices");
		if(verts_node) {
			XMLNode input_node = verts_node.child("input");
			ASSERT(input_node && StringRef(input_node.attrib("semantic")) == "POSITION");
			m_position_source_name = verts_node.attrib("id");
			StringRef target_name = input_node.attrib("source");
			m_position_source = findSource(target_name.c_str());
		}

		m_triangles = Triangles(this, tris_node);
	}

	const Source *Mesh::findSource(const char *id) const {
		ASSERT(id && id[0] == '#');
		if(m_position_source_name == id + 1)
			return m_position_source;
		return Node::findSource(id);
	}

	Skin::Skin(Node *parent, XMLNode node) : Node(parent, node) {
		XMLNode skin_node = m_node.child("skin");
		ASSERT(skin_node);
		parseSources(skin_node);

		m_weights = nullptr;
		m_joints = nullptr;
		m_inv_bind_poses = nullptr;

		m_bind_shape_matrix = identity();
		XMLNode bind_shape_matrix_node = skin_node.child("bind_shape_matrix");
		if(bind_shape_matrix_node) {
			parseValues(bind_shape_matrix_node, &m_bind_shape_matrix[0][0], 16);
			m_bind_shape_matrix = transpose(m_bind_shape_matrix);
		}

		{
			XMLNode joints_node = skin_node.child("joints");
			ASSERT(joints_node);

			XMLNode input = joints_node.child("input");
			while(input) {
				if(input.attrib("semantic") == StringRef("INV_BIND_MATRIX"))
					m_inv_bind_poses = findSource(input.attrib("source"));
				input.next();
			}
		}

		XMLNode vweights_node = skin_node.child("vertex_weights");
		ASSERT(vweights_node);
		XMLNode vcount_node = vweights_node.child("vcount");
		XMLNode v_node = vweights_node.child("v");
		ASSERT(vcount_node && v_node);

		m_counts.resize(vweights_node.attrib<int>("count"));
		parseValues(vcount_node, m_counts.data(), m_counts.size());

		int num_indices = 0;
		for(int n = 0; n < m_counts.size(); n++)
			num_indices += m_counts[n] * 2;
		m_indices.resize(num_indices);
		parseValues(v_node, m_indices.data(), m_indices.size());

		XMLNode input = vweights_node.child("input");
		while(input) {
			if(input.attrib("semantic") == StringRef("JOINT")) {
				m_joint_offset = input.attrib<int>("offset");
				m_joints = findSource(input.attrib("source"));
			} else if(input.attrib("semantic") == StringRef("WEIGHT")) {
				m_weight_offset = input.attrib<int>("offset");
				m_weights = findSource(input.attrib("source"));
			}
			input.next();
		}

		ASSERT(m_inv_bind_poses && m_weights && m_joints);
		ASSERT(m_joints->type() == Source::type_name);
		ASSERT(m_weights->type() == Source::type_float);
		ASSERT(m_inv_bind_poses->type() == Source::type_matrix);
	}

	Animation::Animation(Node *parent, XMLNode node) : Node(parent, node) {
		m_frame_count = -1;

		XMLNode sampler_node = node.child("sampler");
		while(sampler_node) {
			Sampler sampler;
			sampler.id = sampler_node.attrib("id");
			sampler.input = sampler.output = sampler.interpolation = nullptr;

			XMLNode input = sampler_node.child("input");
			while(input) {
				const Source *source = findSource(input.attrib("source"));
				SamplerSemantic::Type type = SamplerSemantic::fromString(input.attrib("semantic"));

				if(type == SamplerSemantic::input)
					sampler.input = source;
				if(type == SamplerSemantic::output)
					sampler.output = source;
				if(type == SamplerSemantic::interpolation)
					sampler.interpolation = source;
				input.next();
			}

			ASSERT(sampler.input && sampler.output);
			ASSERT(sampler.input->type() == Source::type_float);
			ASSERT(sampler.output->type() == Source::type_matrix);

			if(m_frame_count == -1)
				m_frame_count = sampler.input->size();
			ASSERT(sampler.input->size() == m_frame_count);
			ASSERT(sampler.output->size() == m_frame_count);

			if(sampler.interpolation) {
				ASSERT(sampler.interpolation->type() == Source::type_name);
				ASSERT(sampler.interpolation->size() == m_frame_count);
			}

			m_samplers.push_back(sampler);
			sampler_node.next();
		}

		ASSERT(m_frame_count != -1);

		XMLNode channel_node = node.child("channel");
		while(channel_node) {
			Channel channel;

			channel.sampler_id = -1;
			auto source_name = channel_node.attrib("source");
			ASSERT(source_name[0] == '#');
			for(int n = 0; n < (int)m_samplers.size(); n++)
				if(m_samplers[n].id == source_name + 1) {
					channel.sampler_id = n;
					break;
				}

			ASSERT(channel.sampler_id != -1);
			channel.target_name = channel_node.attrib("target");
			m_channels.push_back(channel);
			channel_node.next();
		}
	}

	SceneNode::SceneNode(Node *parent, XMLNode node) : Node(parent, node) {}
}
}
