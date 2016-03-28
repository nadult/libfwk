/* Copyright (C) 2015 - 2016 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#pragma once

#include "fwk_gfx.h"
#include "fwk_xml.h"

namespace fwk {

struct Pose : public immutable_base<Pose> {
  public:
	using NameMap = vector<pair<string, int>>;

	Pose(vector<Matrix4> transforms = {}, NameMap = {});
	Pose(vector<Matrix4> transforms, const vector<string> &names);

	auto size() const { return m_transforms.size(); }
	const auto &transforms() const { return m_transforms; }
	const NameMap &nameMap() const { return m_name_map; }

	vector<int> mapNames(const vector<string> &) const;
	vector<Matrix4> mapTransforms(const vector<int> &mapping) const;

  private:
	NameMap m_name_map;
	vector<Matrix4> m_transforms;
};

using PPose = immutable_ptr<Pose>;

struct MeshBuffers {
  public:
	struct VertexWeight {
		VertexWeight(float weight = 0.0f, int node_id = 0) : weight(weight), node_id(node_id) {}

		float weight;
		int node_id;
	};

	MeshBuffers() = default;
	MeshBuffers(vector<float3> positions, vector<float3> normals = {},
				vector<float2> tex_coords = {}, vector<IColor> colors = {},
				vector<vector<VertexWeight>> weights = {}, vector<string> node_names = {});
	MeshBuffers(PVertexBuffer positions, PVertexBuffer normals = PVertexBuffer(),
				PVertexBuffer tex_coords = PVertexBuffer(), PVertexBuffer colors = PVertexBuffer());
	MeshBuffers(PVertexArray, int positions_id, int normals_id = -1, int tex_coords_id = -1,
				int color_id = -1);

	MeshBuffers(const XMLNode &node);
	void saveToXML(XMLNode) const;

	vector<float3> animatePositions(CRange<Matrix4>) const;
	vector<float3> animateNormals(CRange<Matrix4>) const;

	int size() const { return (int)positions.size(); }
	bool hasSkin() const { return !weights.empty() && !node_names.empty(); }

	MeshBuffers remap(const vector<uint> &mapping) const;

	// Pose must have configuration for each of the nodes in node_names
	vector<Matrix4> mapPose(PPose skinning_pose) const;

	static MeshBuffers transform(const Matrix4 &, MeshBuffers);

	vector<float3> positions;
	vector<float3> normals;
	vector<float2> tex_coords;
	vector<IColor> colors;
	vector<vector<VertexWeight>> weights;
	vector<string> node_names;
};

class MeshIndices {
  public:
	using Type = PrimitiveType;
	using TriIndices = array<uint, 3>;

	static bool isSupported(Type type) {
		return isOneOf(type, Type::triangles, Type::triangle_strip);
	}

	MeshIndices(vector<uint> = {}, Type ptype = Type::triangles);
	MeshIndices(PIndexBuffer, Type ptype = Type::triangles);
	MeshIndices(const vector<TriIndices> &);

	static MeshIndices makeRange(int count, uint first = 0, Type ptype = Type::triangles);

	operator const vector<uint> &() const { return m_data; }
	Type type() const { return m_type; }

	vector<TriIndices> trisIndices() const;

	// Does not exclude degenerate triangles
	int triangleCount() const;
	int size() const { return (int)m_data.size(); }
	bool empty() const { return m_data.empty(); }
	pair<uint, uint> indexRange() const;

	static MeshIndices changeType(MeshIndices, Type new_type);
	static MeshIndices merge(const vector<MeshIndices> &, vector<pair<uint, uint>> &index_ranges);
	static MeshIndices applyOffset(MeshIndices, uint offset);

	vector<MeshIndices> split(uint max_vertices, vector<vector<uint>> &out_mappings) const;

  private:
	vector<uint> m_data;
	Type m_type;
};

class Mesh : public immutable_base<Mesh> {
  public:
	Mesh(MeshBuffers buffers = MeshBuffers(), vector<MeshIndices> indices = {},
		 vector<string> mat_names = {});
	Mesh(const Mesh &) = default;
	Mesh(Mesh &&) = default;
	Mesh &operator=(Mesh &&) = default;
	Mesh &operator=(const Mesh &) = default;

	Mesh(const XMLNode &);
	void saveToXML(XMLNode) const;

	static Mesh makePolySoup(CRange<Triangle>);
	static Mesh makeRect(const FRect &xz_rect, float y);
	static Mesh makeBBox(const FBox &bbox);
	static Mesh makeCylinder(const Cylinder &, int num_sides);
	static Mesh makeTetrahedron(const Tetrahedron &);
	static Mesh makePlane(const Plane &, const float3 &start, float extent);

	struct AnimatedData {
		bool empty() const { return positions.empty(); }

		FBox bounding_box;
		vector<float3> positions;
		vector<float3> normals;
	};

	FBox boundingBox() const { return m_bounding_box; }
	FBox boundingBox(const AnimatedData &) const;

	void changePrimitiveType(PrimitiveType new_type);

	int vertexCount() const { return (int)m_buffers.positions.size(); }
	int triangleCount() const;

	const MeshBuffers &buffers() const { return m_buffers; }
	const vector<float3> &positions() const { return m_buffers.positions; }
	const vector<float3> &normals() const { return m_buffers.normals; }
	const vector<float2> &texCoords() const { return m_buffers.tex_coords; }
	const vector<MeshIndices> &indices() const { return m_indices; }
	const auto &materialNames() const { return m_material_names; }

	bool hasTexCoords() const { return !m_buffers.tex_coords.empty(); }
	bool hasNormals() const { return !m_buffers.normals.empty(); }
	bool hasColors() const { return !m_buffers.colors.empty(); }
	bool hasSkin() const { return m_buffers.hasSkin(); }
	bool hasIndices() const { return !m_indices.empty(); }
	bool empty() const { return m_buffers.positions.empty(); }

	void removeNormals();
	void removeTexCoords();
	void removeColors();
	void removeIndices(CRange<pair<string, IColor>> color_map = {});
	static Mesh transform(const Matrix4 &, Mesh);

	using TriIndices = MeshIndices::TriIndices;
	vector<Triangle> tris() const;
	vector<TriIndices> trisIndices() const;

	vector<Mesh> split(int max_vertices) const;
	static Mesh merge(vector<Mesh>);

	float intersect(const Segment &) const;
	float intersect(const Segment &, const AnimatedData &) const;

	AnimatedData animate(PPose) const;
	static Mesh apply(Mesh, AnimatedData);

	vector<float3> lines() const;
	vector<DrawCall> genDrawCalls(const MaterialSet &, const AnimatedData *anim_data = nullptr,
								  const Matrix4 & = Matrix4::identity()) const;

  protected:
	bool valid(const AnimatedData &) const;

	MeshBuffers m_buffers;
	vector<MeshIndices> m_indices;
	vector<string> m_material_names;
	FBox m_bounding_box;
};

// Vertex / Poly indices can have values up to vertexIdCount() / polyIdCount() -1
// Some indices in the middle may be invalid
class DynamicMesh {
  public:
	DynamicMesh(CRange<float3> verts, CRange<array<uint, 3>> tris, int poly_value = 0);
	DynamicMesh(CRange<float3> verts, CRange<vector<uint>> polys, int poly_value = 0);
	explicit DynamicMesh(const Mesh &mesh) : DynamicMesh(mesh.positions(), mesh.trisIndices()) {}
	DynamicMesh() : DynamicMesh({}, vector<vector<uint>>{}) {}
	explicit operator Mesh() const;

	struct VertexId {
		explicit VertexId(int id = -1) : id(id) {}
		operator int() const { return id; }
		explicit operator bool() const { return isValid(); }
		bool isValid() const { return id >= 0; }
		int id;
	};

	struct PolyId {
		explicit PolyId(int id = -1) : id(id) {}
		operator int() const { return id; }
		explicit operator bool() const { return isValid(); }
		bool isValid() const { return id >= 0; }
		int id;
	};

	struct EdgeId {
		EdgeId() {}
		EdgeId(VertexId a, VertexId b) : a(a), b(b) {}
		bool isValid() const { return a.isValid() && b.isValid() && a != b; }
		explicit operator bool() const { return isValid(); }
		EdgeId inverse() const { return EdgeId(b, a); }
		EdgeId ordered() const { return a < b ? EdgeId(a, b) : EdgeId(b, a); }
		bool operator==(const EdgeId &rhs) const {
			return std::tie(a, b) == std::tie(rhs.a, rhs.b);
		}
		bool operator<(const EdgeId &rhs) const { return std::tie(a, b) < std::tie(rhs.a, rhs.b); }

		bool hasSharedEnds(const EdgeId &other) const {
			return isOneOf(a, other.a, other.b) || isOneOf(b, other.a, other.b);
		}

		VertexId a, b;
	};

	DynamicMesh extract(CRange<PolyId>) const;
	vector<DynamicMesh> separateSurfaces() const;

	bool isValid(VertexId) const;
	bool isValid(PolyId) const;
	bool isValid(EdgeId) const;

	using Polygon = vector<VertexId>;

	class Simplex {
	  public:
		Simplex() : m_size(0) {}
		Simplex(VertexId vert) : m_size(1) { m_verts[0] = vert; }
		Simplex(EdgeId edge) : m_size(2) {
			m_verts[0] = edge.a;
			m_verts[1] = edge.b;
		}
		Simplex(const array<VertexId, 3> &face) : m_verts(face), m_size(3) {}

		int size() const { return m_size; }
		bool isVertex() const { return m_size == 1; }
		bool isEdge() const { return m_size == 2; }
		bool isFace() const { return m_size == 3; }

		VertexId asVertex() const {
			DASSERT(isVertex());
			return m_verts[0];
		}

		EdgeId asEdge() const {
			DASSERT(isEdge());
			return EdgeId(m_verts[0], m_verts[1]);
		}

		array<VertexId, 3> asFace() const {
			DASSERT(isFace());
			return m_verts;
		}

		bool operator<(const Simplex &rhs) const {
			return std::tie(m_size, m_verts) < std::tie(rhs.m_size, rhs.m_verts);
		}
		bool operator==(const Simplex &rhs) const {
			return std::tie(m_size, m_verts) == std::tie(rhs.m_size, rhs.m_verts);
		}

		VertexId operator[](int id) const {
			DASSERT(id >= 0 && id < m_size);
			return m_verts[id];
		}
		string print(const DynamicMesh &mesh) const {
			TextFormatter out;
			out("(");
			for(int i = 0; i < m_size; i++) {
				auto pt = mesh.point(m_verts[i]);
				out("%f:%f:%f%c", pt.x, pt.y, pt.z, i + 1 == m_size ? ')' : ' ');
			}
			return (string)out.text();
		}

	  private:
		array<VertexId, 3> m_verts;
		int m_size;
	};

	bool isValid(const Simplex &simplex) const {
		for(int i = 0; i < simplex.size(); i++)
			if(!isValid(simplex[i]))
				return false;
		return true;
	}

	template <class T1, class T2> bool isValid(const pair<T1, T2> &simplex_pair) const {
		return isValid(simplex_pair.first) && isValid(simplex_pair.second);
	}

	// TODO: we need overlap tests as well
	bool isClosedOrientableSurface(CRange<PolyId>) const;

	// Basically it means that it is a union of closed orientable surfaces
	bool representsVolume() const;
	int eulerPoincare() const;

	bool isTriangular() const;

	VertexId addVertex(const float3 &pos);
	PolyId addPoly(CRange<VertexId, 3>, int value = 0);
	PolyId addPoly(VertexId v0, VertexId v1, VertexId v2, int value = 0) {
		return addPoly({v0, v1, v2}, value);
	}

	void remove(VertexId);
	void remove(PolyId);

	static pair<Simplex, Simplex> makeSimplexPair(const Simplex &a, const Simplex &b) {
		return a < b ? make_pair(b, a) : make_pair(a, b);
	}

	template <class TSimplex> auto nearbyVerts(TSimplex simplex_id, float tolerance) const {
		vector<pair<Simplex, Simplex>> out;
		DASSERT(isValid(simplex_id));

		for(auto vert : verts())
			if(!coincident(vert, simplex_id) &&
			   distance(simplex(simplex_id), point(vert)) < tolerance)
				out.emplace_back(makeSimplexPair(simplex_id, vert));
		return out;
	}

	template <class TSimplex> auto nearbyEdges(TSimplex simplex_id, float tolerance) const {
		vector<pair<Simplex, Simplex>> out;
		DASSERT(isValid(simplex_id));

		for(auto edge : edges())
			if(!coincident(simplex_id, edge) &&
			   distance(simplex(simplex_id), segment(edge)) < tolerance)
				out.emplace_back(makeSimplexPair(simplex_id, edge));
		return out;
	}

	template <class TSimplex> auto nearbyPairs(TSimplex simplex_id, float tolerance) const {
		DASSERT(isValid(simplex_id));

		vector<pair<Simplex, Simplex>> out;
		insertBack(out, nearbyVerts(simplex_id, tolerance));
		insertBack(out, nearbyEdges(simplex_id, tolerance));
		return out;
	}

	static DynamicMesh merge(CRange<DynamicMesh>);

	VertexId merge(CRange<VertexId>);
	VertexId merge(CRange<VertexId>, const float3 &target_pos);

	void split(EdgeId, VertexId);
	void move(VertexId, const float3 &new_pos);

	vector<PolyId> inverse(CRange<PolyId>) const;
	vector<VertexId> inverse(CRange<VertexId>) const;

	vector<VertexId> verts() const;
	vector<VertexId> verts(CRange<PolyId>) const;
	vector<VertexId> verts(PolyId) const;
	array<VertexId, 2> verts(EdgeId) const;

	vector<PolyId> polys() const;
	vector<PolyId> polys(VertexId) const;
	vector<PolyId> polys(EdgeId) const;
	vector<PolyId> coincidentPolys(PolyId) const;

	template <class Filter> vector<PolyId> polys(VertexId vertex, const Filter &filter) const {
		return fwk::filter(polys(vertex), filter);
	}

	template <class Filter> vector<PolyId> polys(EdgeId edge, const Filter &filter) const {
		return fwk::filter(polys(edge), filter);
	}

	bool coincident(VertexId vert1, VertexId vert2) const { return vert1 == vert2; }
	bool coincident(VertexId vert, EdgeId edge) const;
	bool coincident(EdgeId edge1, EdgeId edge2) const;
	bool coincident(VertexId vert, PolyId face) const;
	bool coincident(EdgeId, PolyId) const;
	bool coincident(PolyId, PolyId) const;

	vector<PolyId> selectSurface(PolyId representative) const;

	vector<EdgeId> edges() const;
	vector<EdgeId> edges(PolyId) const;

	EdgeId polyEdge(PolyId face_id, int sub_id) const;
	int polyEdgeIndex(PolyId, EdgeId) const;
	VertexId otherVertex(PolyId, EdgeId) const;

	// All edges starting from current vertex
	vector<EdgeId> edges(VertexId) const;

	float3 point(VertexId id) const {
		DASSERT(isValid(id));
		return m_verts[id];
	}

	FBox box(EdgeId) const;
	Segment segment(EdgeId) const;
	Triangle triangle(PolyId) const;

	float3 simplex(VertexId id) const { return point(id); }
	Segment simplex(EdgeId id) const { return segment(id); }
	Triangle simplex(PolyId id) const { return triangle(id); }

	template <class TSimplex> float sdistance(const Simplex &gsimplex, TSimplex tsimplex) const {
		if(gsimplex.isVertex())
			return distance(point(gsimplex.asVertex()), simplex(tsimplex));
		else if(gsimplex.isEdge())
			return distance(segment(gsimplex.asEdge()), simplex(tsimplex));
		else
			THROW("simplex type not supported");
		return constant::inf;
	}

	float sdistance(const Simplex &a, const Simplex &b) const {
		if(b.isVertex())
			return sdistance(a, b.asVertex());
		else if(b.isEdge())
			return sdistance(a, b.asEdge());
		else
			THROW("simplex type not supported");
		return constant::inf;
	}

	Projection edgeProjection(EdgeId, PolyId) const;

	template <class Simplex, class MeshSimplex = VertexId>
	auto closestVertex(const Simplex &simplex, MeshSimplex exclude = VertexId()) const {
		VertexId out;
		float min_dist = constant::inf;

		for(auto vert : verts()) {
			if(coincident(exclude, vert))
				continue;
			float dist = distance(simplex, point(vert));
			if(dist < min_dist) {
				out = vert;
				min_dist = dist;
			}
		}

		return out;
	}

	template <class Simplex, class MeshSimplex = EdgeId>
	auto closestEdge(const Simplex &simplex, MeshSimplex exclude = EdgeId()) const {
		EdgeId out;
		float min_dist = constant::inf;

		for(auto edge : edges()) {
			if(coincident(exclude, edge))
				continue;
			if(distance(point(edge.a), point(edge.b)) < constant::epsilon)
				xmlPrint("Invalid edge: % - % (%) (%)\n", edge.a.id, edge.b.id, point(edge.a),
						 point(edge.b));
			float dist = distance(simplex, segment(edge));
			if(dist < min_dist) {
				out = edge;
				min_dist = dist;
			}
		}

		return out;
	}

	vector<PolyId> triangulate(PolyId);

	// When faces are modified, or divided, their values are propagated
	int value(PolyId) const;
	void setValue(PolyId, int value);

	int polyCount(VertexId) const;
	int polyCount() const { return m_num_polys; }

	int vertexCount() const { return m_num_verts; }
	int vertexCount(PolyId) const;

	int vertexIdCount() const { return (int)m_verts.size(); }
	int polyIdCount() const { return (int)m_polys.size(); }

  private:
	struct Poly {
		vector<int> verts;
		int value;
	};
	vector<float3> m_verts;
	vector<Poly> m_polys;
	vector<vector<int>> m_adjacency;
	vector<int> m_free_verts, m_free_polys;
	int m_num_verts, m_num_polys;
};

float3 closestPoint(const DynamicMesh &, const float3 &point);

using PMesh = immutable_ptr<Mesh>;

struct MaterialDef {
	MaterialDef(const string &name, FColor diffuse) : name(name), diffuse(diffuse) {}
	MaterialDef(const XMLNode &);
	void saveToXML(XMLNode) const;

	string name;
	FColor diffuse;
};

class Model;

class ModelAnim {
  public:
	ModelAnim(const XMLNode &, PPose default_pose);
	void saveToXML(XMLNode) const;

	string print() const;
	const string &name() const { return m_name; }
	float length() const { return m_length; }
	// TODO: advanced interpolation support

	static void transToXML(const AffineTrans &trans, const AffineTrans &default_trans,
						   XMLNode node);
	static AffineTrans transFromXML(XMLNode node, const AffineTrans &default_trans = AffineTrans());

	PPose animatePose(PPose initial_pose, double anim_time) const;

  protected:
	string m_name;

	AffineTrans animateChannel(int channel_id, double anim_pos) const;
	void verifyData() const;

	struct Channel {
		Channel() = default;
		Channel(const XMLNode &, const AffineTrans &default_trans);
		void saveToXML(XMLNode) const;
		AffineTrans blend(int frame0, int frame1, float t) const;

		// TODO: interpolation information
		AffineTrans trans, default_trans;
		vector<float3> translation_track;
		vector<float3> scaling_track;
		vector<Quat> rotation_track;
		vector<float> time_track;
		string node_name;
	};

	// TODO: information about whether its looped or not
	vector<Channel> m_channels;
	vector<float> m_shared_time_track;
	vector<string> m_node_names;
	float m_length;
};

class ModelNode;
using PModelNode = unique_ptr<ModelNode>;

DEFINE_ENUM(ModelNodeType, generic, mesh, armature, bone, empty);

class ModelNode {
  public:
	using Type = ModelNodeType;

	struct Property {
		bool operator<(const Property &rhs) const {
			return tie(name, value) < tie(rhs.name, rhs.value);
		}

		string name;
		string value;
	};

	using PropertyMap = std::map<string, string>;

	ModelNode(const string &name, Type type = Type::generic,
			  const AffineTrans &trans = AffineTrans(), PMesh mesh = PMesh(),
			  vector<Property> props = {});
	ModelNode(const ModelNode &rhs);

	void addChild(PModelNode);
	PModelNode removeChild(const ModelNode *);
	PModelNode clone() const;

	Type type() const { return m_type; }
	const auto &children() const { return m_children; }
	const auto &name() const { return m_name; }
	const auto &properties() const { return m_properties; }
	PropertyMap propertyMap() const;
	const ModelNode *find(const string &name, bool recursive = true) const;

	// TODO: name validation
	void setTrans(const AffineTrans &trans);
	void setName(const string &name) { m_name = name; }
	void setMesh(PMesh mesh) { m_mesh = move(mesh); }
	void setId(int new_id) { m_id = new_id; }

	const auto &localTrans() const { return m_trans; }
	const auto &invLocalTrans() const { return m_inv_trans; }

	Matrix4 globalTrans() const;
	Matrix4 invGlobalTrans() const;

	auto mesh() const { return m_mesh; }
	int id() const { return m_id; }

	const ModelNode *root() const;
	const ModelNode *parent() const { return m_parent; }
	bool isDescendant(const ModelNode *ancestor) const;

	void dfs(vector<ModelNode *> &out);
	bool join(const ModelNode *other, const string &name);

  private:
	vector<PModelNode> m_children;
	vector<Property> m_properties;
	string m_name;
	AffineTrans m_trans;
	Matrix4 m_inv_trans;
	PMesh m_mesh;
	Type m_type;
	int m_id;
	const ModelNode *m_parent;
};

class RenderList;

class Model : public immutable_base<Model> {
  public:
	Model() = default;
	Model(Model &&) = default;
	Model(const Model &);
	Model &operator=(Model &&) = default;
	~Model() = default;

	Model(PModelNode, vector<ModelAnim> anims = {}, vector<MaterialDef> material_defs = {});
	static Model loadFromXML(const XMLNode &);
	void saveToXML(XMLNode) const;

	// TODO: use this in transformation functions
	// TODO: better name
	/*	void decimate(MeshBuffers &out_buffers, vector<MeshIndices> &out_indices,
					  vector<string> &out_names) {
			out_buffers = move(m_buffers);
			out_indices = move(m_indices);
			out_names = move(m_material_names);
			*this = Mesh();
		}*/

	void join(const string &local_name, const Model &, const string &other_name);

	const ModelNode *findNode(const string &name) const { return m_root->find(name); }
	int findNodeId(const string &name) const;
	const ModelNode *rootNode() const { return m_root.get(); }
	const auto &nodes() const { return m_nodes; }
	const auto &anims() const { return m_anims; }
	const auto &materialDefs() const { return m_material_defs; }

	void printHierarchy() const;

	Matrix4 nodeTrans(const string &name, PPose) const;

	void drawNodes(RenderList &, PPose, FColor node_color, FColor line_color,
				   float node_scale = 1.0f, const Matrix4 &matrix = Matrix4::identity()) const;

	void clearDrawingCache() const;

	const ModelAnim &anim(int anim_id) const { return m_anims[anim_id]; }
	int animCount() const { return (int)m_anims.size(); }

	// Pass -1 to anim_id for bind position
	PPose animatePose(int anim_id, double anim_pos) const;
	PPose defaultPose() const { return m_default_pose; }
	PPose globalPose(PPose local_pose) const;
	PPose meshSkinningPose(PPose global_pose, int node_id) const;
	bool valid(PPose) const;

  protected:
	void updateNodes();

	PModelNode m_root;
	vector<ModelAnim> m_anims;
	vector<MaterialDef> m_material_defs;
	vector<ModelNode *> m_nodes;
	PPose m_default_pose;
};

class AnimatedModel {
  public:
	struct MeshData {
		PMesh mesh;
		Mesh::AnimatedData anim_data;
		Matrix4 transform;
	};

	AnimatedModel(vector<MeshData>);
	AnimatedModel(const Model &, PPose = PPose());

	Mesh toMesh() const;
	FBox boundingBox() const;
	float intersect(const Segment &) const;
	vector<DrawCall> genDrawCalls(const MaterialSet &, const Matrix4 & = Matrix4::identity()) const;

  private:
	vector<MeshData> m_meshes;
};

using PModel = immutable_ptr<Model>;

template <class T> class XMLLoader : public ResourceLoader<T> {
  public:
	XMLLoader(const string &prefix, const string &suffix, string node_name)
		: ResourceLoader<T>(prefix, suffix), m_node_name(move(node_name)) {}

	immutable_ptr<T> operator()(const string &name) {
		XMLDocument doc;
		Loader(ResourceLoader<T>::fileName(name)) >> doc;
		XMLNode child = doc.child(m_node_name.empty() ? nullptr : m_node_name.c_str());
		if(!child)
			THROW("Cannot find node '%s' in XML document", m_node_name.c_str());
		return immutable_ptr<T>(T::loadFromXML(child));
	}

  protected:
	string m_node_name;
};
}
