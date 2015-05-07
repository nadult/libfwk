/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#ifndef FWK_COLLADA_H
#define FWK_COLLADA_H

#include "fwk_base.h"
#include "fwk_math.h"
#include "fwk_xml.h"

namespace fwk {
namespace collada {

	DECLARE_ENUM(Semantic, vertex, normal, color, tex_coord, tex_tangent, tex_binormal,

				 weight, joint, inv_bind_matrix);

	DECLARE_ENUM(SamplerSemantic, input, output, interpolation, in_tangent, out_tangent);

	DECLARE_ENUM(SourceArrayType, idref_array, name_array, bool_array, float_array, int_array);

	class Node;

	class Source {
	  public:
		Source(XMLNode);

		enum Type {
			type_name,
			type_float,
			type_float2,
			type_float3,
			type_float4,
			type_matrix,

			type_unknown,
		};

		StringRef id() const { return m_id; }
		Type type() const { return m_type; }

		int size() const { return m_count; }

		float toFloat(int idx) const;
		float2 toFloat2(int idx) const;
		float3 toFloat3(int idx) const;
		float4 toFloat4(int idx) const;

		Matrix4 toMatrix(int idx) const;
		StringRef toString(int idx) const;

	  protected:
		template <class T, class TBase> const T get(int idx) const;

		PodArray<char> m_array;
		vector<string> m_string_array;
		StringRef m_id;
		int m_stride, m_offset, m_count;
		int m_array_count;
		SourceArrayType::Type m_array_type;
		Type m_type;
	};

	class Triangles {
	  public:
		Triangles(Node *parent, XMLNode);
		Triangles();

		StringRef materialName() const { return m_material_name; }
		int count() const { return m_vertex_count / 3; }
		int vertexCount() const { return m_vertex_count; }

		int attribIndex(Semantic::Type sem, int idx) const;
		const Source *attribSource(Semantic::Type sem) const { return m_sources[sem]; }
		bool hasAttrib(Semantic::Type sem) const { return m_sources[sem] != nullptr; }

	  protected:
		Node *m_parent;
		PodArray<int> m_indices;
		StringRef m_material_name;
		int m_offsets[Semantic::count];
		const Source *m_sources[Semantic::count];
		int m_vertex_count, m_stride;
	};

	class Node {
	  public:
		enum TypeId {
			type_root,
			type_mesh,
			type_skin,
			type_animation,
			type_root_joint,
		};

		Node(Node *parent, XMLNode node);
		//TODO: fix copying
//		Node(const Node &) = delete;
//		void operator=(const Node &) = delete;

		virtual ~Node() {}
		virtual TypeId typeId() const = 0;
		StringRef id() const { return m_id; }

		virtual const Source *findSource(const char *id) const;
		XMLNode node() const { return m_node; }

	  protected:
		void parseSources(XMLNode);

		XMLNode m_node;

	  private:
		Node *m_parent;
		StringRef m_id;
		vector<Source> m_sources;
	};

	class Mesh : public Node {
	  public:
		Mesh(Node *parent, XMLNode);
		TypeId typeId() const { return type_mesh; }

		virtual const Source *findSource(const char *id) const;
		const Triangles triangles() const { return m_triangles; }

	  protected:
		Triangles m_triangles;
		const Source *m_position_source;
		StringRef m_position_source_name;
	};

	class Animation : public Node {
	  public:
		Animation(Node *parent, XMLNode);
		TypeId typeId() const { return type_animation; }

		struct Sampler {
			StringRef id;
			const Source *input;		 // float
			const Source *output;		 // matrix
			const Source *interpolation; // name, optional
		};

		struct Channel {
			int sampler_id;
			StringRef target_name;
		};

		int m_frame_count;
		vector<Sampler> m_samplers;
		vector<Channel> m_channels;
	};

	class Skin : public Node {
	  public:
		Skin(Node *parent, XMLNode);
		TypeId typeId() const { return type_skin; }

		Matrix4 m_bind_shape_matrix;
		const Source *m_weights;		// float
		const Source *m_joints;			// name
		const Source *m_inv_bind_poses; // matrix
		int m_joint_offset, m_weight_offset;
		PodArray<int> m_counts, m_indices;
	};

	class SceneNode : public Node {
	  public:
		SceneNode(Node *parent, XMLNode);
		TypeId typeId() const { return type_root_joint; }
	};

	class Root : public Node {
	  public:
		Root(const XMLDocument &doc);
		TypeId typeId() const { return type_root; }

		int upAxis() const { return m_up_axis; }

		// TODO: make Z default (or pass as an argument)
		void fixUpAxis(Matrix4 &, int target_up_axis) const;
		void fixUpAxis(float3 &, int target_up_axis) const;

		int meshCount() const { return (int)m_meshes.size(); }
		int skinCount() const { return (int)m_skins.size(); }
		int animCount() const { return (int)m_anims.size(); }
		int sceneNodeCount() const { return (int)m_root_joints.size(); }

		const Mesh &mesh(int idx) const { return *m_meshes[idx]; }
		const Skin &skin(int idx) const { return *m_skins[idx]; }
		const Animation &anim(int idx) const { return *m_anims[idx]; }
		const SceneNode &sceneNode(int idx) const { return *m_root_joints[idx]; }

		void printInfo() const;

	  protected:
		vector<shared_ptr<Mesh>> m_meshes;
		vector<shared_ptr<Animation>> m_anims;
		vector<shared_ptr<Skin>> m_skins;
		vector<shared_ptr<SceneNode>> m_root_joints;
		int m_up_axis;
	};
}
}

#endif
