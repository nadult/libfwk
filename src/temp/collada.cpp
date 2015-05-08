/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_collada.h"
#include <algorithm>

namespace fwk {
namespace collada {

	DEFINE_ENUM(Semantic, "VERTEX", "NORMAL", "COLOR", "TEXCOORD", "TEXTANGENT", "TEXBINORMAL",

				"WEIGHT", "JOINT", "INV_BIND_MATRIX");

	DEFINE_ENUM(SamplerSemantic, "INPUT", "OUTPUT", "INTERPOLATION", "IN_TANGENT", "OUT_TANGENT");

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

	static string parseRef(const char *str) {
		DASSERT(str);
		ASSERT(str[0] == '#');
		return string(str + 1);
	}

	void parseValues(XMLNode node, int *out, int count) { parseValues(node, out, count, atoi); }
	void parseValues(XMLNode node, float *out, int count) { parseValues(node, out, count, atof); }
	void parseValues(XMLNode node, bool *out, int count) {
		parseValues<bool>(node, out, count,
						  [](const char *v) { return strcmp(v, "true") == 0 ? true : false; });
	}

	Source::Source(XMLNode node) : m_type(type_unknown) {
		DASSERT(node && StringRef(node.name()) == "source");
		m_id = node.attrib("id");

		XMLNode tech_node = node.child("technique_common");
		ASSERT(tech_node);
		XMLNode accessor_node = tech_node.child("accessor");
		ASSERT(accessor_node && !accessor_node.sibling("accessor"));

		string acc_source = parseRef(accessor_node.attrib("source"));
		int acc_stride = accessor_node.attrib<int>("stride", 1);
		int acc_offset = accessor_node.attrib<int>("offset", 0);
		int acc_count = accessor_node.attrib<int>("count");

		vector<string> acc_types;
		{
			XMLNode param_node = accessor_node.child("param");
			while(param_node) {
				acc_types.emplace_back(param_node.attrib("type"));
				param_node.next();
			}
		}

		XMLNode farray_node = node.child("float_array");
		XMLNode sarray_node = node.child("Name_array");

		if(farray_node) {
			m_floats = xml_conversions::fromString<vector<float>>(farray_node.value());
			ASSERT((int)m_floats.size() == farray_node.attrib<int>("count"));
			ASSERT(acc_source == farray_node.attrib("id"));

			if(acc_types.size() == 1 && acc_types[0] == "float4x4") {
				ASSERT(acc_stride == 16);
				ASSERT(acc_count == (int)m_floats.size() / acc_stride);
				m_type = type_matrix4;
			} else {
				ASSERT(std::all_of(acc_types.begin(), acc_types.end(),
								   [](const string &s) { return s == "float"; }));
				int size = (int)acc_types.size();
				ASSERT(size >= 1 && size <= 4);
				ASSERT(acc_count == (int)m_floats.size() / size);
				ASSERT(acc_stride == size);

				m_type = size == 1 ? type_float : size == 2 ? type_float2 : size == 3 ? type_float3
																					  : type_float4;
			}
		}
	}

	vector<float> Source::toFloatArray() const {
		DASSERT(m_type == type_float);
		return m_floats;
	}

	vector<float2> Source::toFloat2Array() const {
		DASSERT(m_type == type_float2);
		vector<float2> out;
		out.reserve(m_floats.size() / 2);
		for(int n = 0; n < (int)m_floats.size(); n += 2)
			out.emplace_back(m_floats[n + 0], m_floats[n + 1]);
		return out;
	}

	vector<float3> Source::toFloat3Array() const {
		DASSERT(m_type == type_float3);
		vector<float3> out;
		out.reserve(m_floats.size() / 3);
		for(int n = 0; n < (int)m_floats.size(); n += 3)
			out.emplace_back(m_floats[n + 0], m_floats[n + 1], m_floats[n + 2]);
		return out;
	}

	vector<float4> Source::toFloat4Array() const {
		DASSERT(m_type == type_float4);
		vector<float4> out;
		out.reserve(m_floats.size() / 4);
		for(int n = 0; n < (int)m_floats.size(); n += 4)
			out.emplace_back(m_floats[n + 0], m_floats[n + 1], m_floats[n + 2], m_floats[n + 3]);
		return out;
	}

	vector<Matrix4> Source::toMatrix4Array() const {
		DASSERT(m_type == type_matrix);
		vector<Matrix4> out;
		out.reserve(m_floats.size() / 16);
		for(int n = 0; n < (int)m_floats.size(); n += 16) {
			const float *ptr = &m_floats[n];
			out.emplace_back(float4(ptr[0], ptr[1], ptr[2], ptr[3]),
							 float4(ptr[4], ptr[5], ptr[6], ptr[7]),
							 float4(ptr[8], ptr[9], ptr[10], ptr[11]),
							 float4(ptr[12], ptr[13], ptr[14], ptr[15]));
		}
		return out;
	}

	vector<string> Source::toNameArray() const { DASSERT(m_type == type_name); return m_strings; }

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
		DASSERT(parent && node);
		bool is_poly_list = StringRef(node.name()) == "polylist";

		m_material_name = node.hasAttrib("material") ? node.attrib("material") : "";
		int poly_count = node.attrib<int>("count");
		m_vertex_count = poly_count * 3;

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

		vector<int> vcounts(poly_count);

		if(is_poly_list) {
			XMLNode vcounts_node = node.child("vcount");
			ASSERT(vcounts_node);
			parseValues(vcounts_node, vcounts.data(), vcounts.size());
			m_vertex_count = 0;
			for(int vc : vcounts)
				m_vertex_count += vc;
		}

		m_indices.resize(m_vertex_count * m_stride);
		parseValues(indices_node, m_indices.data(), m_indices.size());

		if(is_poly_list) {
			XMLNode vcounts_node = node.child("vcount");
			ASSERT(vcounts_node);

			vector<int> new_indices;
			int index = 0;
			for(int vcount : vcounts) {
				if(vcount == 3) {
					for(int i = 0; i < 3 * m_stride; i++)
						new_indices.emplace_back(m_indices[index + i]);
					index += 3 * m_stride;
				} else if(vcount == 4) {
					int remap[6] = {0, 1, 2, 0, 2, 3};
					for(int i = 0; i < 6; i++)
						for(int j = 0; j < m_stride; j++)
							new_indices.emplace_back(m_indices[index + remap[i] * m_stride + j]);
					index += 4 * m_stride;
				} else
					THROW("polylist with vcount > 4 not supported");
			}
			m_vertex_count = new_indices.size() / m_stride;
			m_indices.resize(new_indices.size());
			memcpy(m_indices.data(), new_indices.data(), m_indices.size() * sizeof(m_indices[0]));
		}
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
				m_meshes.push_back(make_shared<Mesh>(this, geometry_node));
				geometry_node.next();
			}
		}
		if(lib_controllers) {
			XMLNode skin_node = lib_controllers.child("controller");
			while(skin_node) {
				m_skins.push_back(make_shared<Skin>(this, skin_node));
				skin_node.next();
			}
		}
		if(lib_animations) {
			XMLNode anim_node = lib_animations.child("animation");
			while(anim_node) {
				m_anims.push_back(make_shared<Animation>(this, anim_node));
				anim_node.next();
			}
		}
		if(lib_visual_scenes) {
			XMLNode visual_scene = lib_visual_scenes.child("visual_scene");
			while(visual_scene) {
				XMLNode node = visual_scene.child("node");

				while(node) {
					m_root_joints.push_back(make_shared<SceneNode>(this, node));
					node.next();
				}
				visual_scene.next();
			}
		}
	}

	static int thirdAxis(int a, int b) {
		int c = (a + 1) % 3;
		if(c == b)
			c = (c + 1) % 3;
		return c;
	}

	void Root::fixUpAxis(Matrix4 &mat, int target_axis) const {
		if(m_up_axis != target_axis) {
			int other_axis = thirdAxis(target_axis, m_up_axis);
			swap(mat[target_axis], mat[m_up_axis]);
			mat[other_axis] *= -1.0f;
			mat = transpose(mat);
			mat[other_axis] *= -1.0f;
			swap(mat[target_axis], mat[m_up_axis]);
			mat = transpose(mat);
		}
	}

	void Root::fixUpAxis(float3 &vec, int target_axis) const {
		if(m_up_axis != target_axis) {
			int other_axis = thirdAxis(target_axis, m_up_axis);
			swap(vec[target_axis], vec[m_up_axis]);
			vec[other_axis] *= -1.0f;
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
		if(!tris_node)
			tris_node = node.child("polylist");
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

		m_bind_shape_matrix = Matrix4::identity();
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
		ASSERT(m_inv_bind_poses->type() == Source::type_matrix4);
	}

	Animation::Animation(Node *parent, XMLNode node) : Node(parent, node) {
		m_frame_count = -1;
		return;

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
			ASSERT(sampler.output->type() == Source::type_matrix4);

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
	
	/*
SimpleMeshData::SimpleMeshData(const collada::Mesh &cmesh) {
using namespace collada;

const Triangles &ctris = cmesh.triangles();
int vertex_count = ctris.count() * 3;
m_positions.resize(vertex_count);
m_normals.resize(vertex_count);
m_tex_coords.resize(vertex_count);

const Source *vertex_source = ctris.attribSource(Semantic::vertex);
const Source *normal_source = ctris.attribSource(Semantic::normal);
const Source *tex_coord_source = ctris.attribSource(Semantic::tex_coord);
ASSERT(vertex_source->type() == Source::type_float3);
ASSERT(normal_source->type() == Source::type_float3);

for(int v = 0; v < vertex_count; v++) {
	m_positions[v] = vertex_source->toFloat3(ctris.attribIndex(Semantic::vertex, v));
	m_normals[v] = normal_source->toFloat3(ctris.attribIndex(Semantic::normal, v));
}

if(tex_coord_source) {
	if(tex_coord_source->type() == Source::type_float3) {
		for(int v = 0; v < vertex_count; v++) {
			float3 uvw = tex_coord_source->toFloat3(ctris.attribIndex(Semantic::tex_coord, v));
			m_tex_coords[v] = float2(uvw.x, -uvw.y);
		}
	} else if(tex_coord_source->type() == Source::type_float2) {
		for(int v = 0; v < vertex_count; v++) {
			float2 uv = tex_coord_source->toFloat2(ctris.attribIndex(Semantic::tex_coord, v));
			m_tex_coords[v] = float2(uv.x, -uv.y);
		}
	}
} else if(!m_tex_coords.empty())
	memset(m_tex_coords.data(), 0, m_tex_coords.size() * sizeof(m_tex_coords[0]));

m_indices.resize(m_positions.size());
for(int n = 0; n < m_indices.size(); n++)
	m_indices[n] = n;

m_primitive_type = PrimitiveType::triangles;
	Matrix4 root_matrix = getRootMatrix(root, cmesh.id());
	Matrix4 nrm_matrix = inverse(transpose(root_matrix));

	for(int n = 0; n < m_positions.size(); n++) {
		m_positions[n] = mulPoint(root_matrix, m_positions[n]);
		root.fixUpAxis(m_positions[n], 2);
	}
	for(int n = 0; n < m_normals.size(); n++) {
		m_normals[n] = mulNormal(nrm_matrix, m_normals[n]);
		root.fixUpAxis(m_normals[n], 2);
	}
	if(root.upAxis() != 0) {
		// TODO: order of vertices in a triangle sometimes has to be changed
		for(int n = 0; n < m_positions.size(); n += 3) {
			//				swap(m_positions[n], m_positions[n + 1]);
			//				swap(m_tex_coords[n], m_tex_coords[n + 1]);
			//				swap(m_normals[n], m_normals[n + 1]);
		}
	}

m_bounding_box = FBox(m_positions.data(), m_positions.size());
}*/


}
}
