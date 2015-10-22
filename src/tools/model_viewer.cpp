/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk.h"

using namespace fwk;

namespace {
ResourceManager<Model, XMLLoader<Model>> s_models("", "", "");
ResourceManager<DTexture> s_textures("", "");

string dataPath(string file_name) {
	FilePath exec(executablePath());
	return exec.parent().parent() / "data" / file_name;
}
}

struct ViewConfig {
	ViewConfig(float zoom = 1.0f, Quat rot = Quat()) : zoom(zoom), rot(rot) {}

	float zoom;
	Quat rot;
};

ViewConfig lerp(const ViewConfig &a, const ViewConfig &b, float t) {
	return ViewConfig(lerp(a.zoom, b.zoom, t), slerp(a.rot, b.rot, t));
}

DECLARE_ENUM(ViewerMode, model, tets, model_csg, tets_csg);
DEFINE_ENUM(ViewerMode, "animated model", "tetrahedralization", "model csg", "tetrahedra csg");

class Viewer {
  public:
	struct Model {
		Model(PModel model, PMaterial default_mat, PTexture tex, string mname, string tname)
			: m_model(std::move(model)), m_materials(default_mat), m_model_name(mname),
			  m_tex_name(tname), m_num_segments(0), m_num_non_manifold(0), m_tets_computed(false) {
			std::map<string, PMaterial> map;

			for(const auto &def : m_model->materialDefs()) {
				map[def.name] = tex ? make_immutable<Material>(tex, def.diffuse)
									: make_immutable<Material>(def.diffuse);
			}
			m_materials = MaterialSet(default_mat, map);
		}

		PPose animatePose(int anim_id, float anim_pos) const {
			return m_model->animatePose(anim_id, anim_pos);
		}

		FBox boundingBox(PPose pose = PPose()) const { return m_model->boundingBox(pose); }

		float scale() const {
			auto bbox = m_model->boundingBox();
			return 4.0f / max(bbox.width(), max(bbox.height(), bbox.depth()));
		}

		void updateTets() {
			if(m_tets_computed)
				return;
			m_tets_computed = true;

			FWK_PROFILE_RARE("XupdateTets");
			m_tet_mesh = PTetMesh();
			auto sub_meshes = DynamicMesh(m_model->toMesh(PPose())).separateSurfaces();

			vector<TetMesh> tets;
			vector<Mesh> isects;

			m_num_segments = m_num_non_manifold = 0;
			for(const auto &sub_mesh : sub_meshes) {
				m_num_segments++;
				if(!DynamicMesh(sub_mesh).representsVolume()) {
					m_num_non_manifold++;
					continue;
				}

				try {
					auto tet = TetMesh::make(Mesh(sub_mesh), 0);
					tets.emplace_back(std::move(tet));
				} catch(...) {
					isects.emplace_back(TetMesh::findIntersections(Mesh(sub_mesh)));
				}
			}

			m_tet_mesh = PTetMesh(TetMesh::merge(tets));
			m_tet_isects = isects.empty() ? PMesh() : PMesh(Mesh::merge(isects));
		}

		void drawModel(Renderer &out, PPose pose, bool show_nodes, const Matrix4 &matrix) {
			m_model->draw(out, pose, m_materials, matrix);
			if(show_nodes)
				m_model->drawNodes(out, pose, Color::green, Color::yellow, 0.1f, matrix);
		}

		void printModelStats(TextFormatter &fmt) const {
			int num_parts = 0, num_verts = 0, num_faces = 0;
			for(const auto &node : m_model->nodes())
				if(node->mesh()) {
					num_parts++;
					num_verts += node->mesh()->vertexCount();
					num_faces += node->mesh()->triangleCount();
				}
			FBox bbox = m_model->boundingBox();
			fmt("Size: %.2f %.2f %.2f\n\n", bbox.width(), bbox.height(), bbox.depth());
			fmt("Parts: %d  Verts: %d Faces: %d\n", num_parts, num_verts, num_faces);
		}

		void printTetStats(TextFormatter &fmt) const {
			DASSERT(m_tets_computed);

			fmt("Segments: %d", m_num_segments);
			if(m_num_non_manifold)
				fmt(" (non manifold: %d)", m_num_non_manifold);
			fmt("\n");

			if(m_tet_mesh)
				fmt("Tets: %d\n", m_tet_mesh->size());
			if(m_tet_isects)
				fmt("Tetrahedralizer error: self-intersections found\n");
		}

		void drawTets(const TetMesh &mesh, Renderer &out, const Matrix4 &matrix,
					  Color color) const {
			PMaterial line_mat = make_immutable<Material>(
				Color::white, Material::flag_blended | Material::flag_ignore_depth);
			PMaterial tet_mat = make_immutable<Material>(
				color, color.a != 255 ? Material::flag_blended | Material::flag_ignore_depth : 0);
			// m_tet_mesh->drawLines(*m_renderer_3d, line_mat, matrix);
			mesh.drawTets(out, tet_mat, matrix);
			// m_tet_mesh->toMesh().draw(*m_renderer_3d, material, matrix);
		}

		void drawTets(Renderer &out, const Matrix4 &matrix, Color color) const {
			DASSERT(m_tets_computed);

			if(m_tet_mesh)
				drawTets(*m_tet_mesh, out, matrix, color);
			if(m_tet_isects) {
				auto material = make_immutable<Material>(
					Color::red, Material::flag_ignore_depth | Material::flag_clear_depth);
				m_tet_isects->draw(out, material, matrix);
				vector<float3> lines;
				for(auto tri : m_tet_isects->tris())
					insertBack(lines, {tri[0], tri[1], tri[1], tri[2], tri[2], tri[0]});
				PMaterial mat = Material(Color::black, Material::flag_ignore_depth);
				out.addLines(lines, mat, matrix);
			}
		}

		auto makeMat(Color col, bool line) {
			uint flags =
				col.a != 255 || line ? Material::flag_blended | Material::flag_ignore_depth : 0;
			return make_immutable<Material>(col, flags);
		}

		void drawTetsCsg(Renderer &out, const Matrix4 &matrix, Color color, float3 offset,
						 float scale, int num_steps, int csg_phase, int csg_mesh_id) {
			if(!m_tet_mesh)
				return;

			PMaterial tri_mat = Material(Color::red);
			PMaterial line_mat = Material(Color::black, Material::flag_ignore_depth);
			PMaterial line2_mat = Material(Color::yellow, Material::flag_ignore_depth);

			auto second_mesh = PTetMesh(TetMesh::transform(translation(offset), *m_tet_mesh));
			vector<Segment> segments;
			vector<Triangle> tris;
			vector<Tetrahedron> tets;
			vector<vector<Triangle>> segs;
			TetMesh final, final_removed;

			auto mesh_a = csg_mesh_id ? *second_mesh : *m_tet_mesh;
			auto mesh_b = csg_mesh_id ? *m_tet_mesh : *second_mesh;
			TetMesh::CSGVisualData vis_data;
			vis_data.max_steps = num_steps;
			vis_data.phase = csg_phase;

			auto csg = PTetMesh(TetMesh::csg(mesh_a, mesh_b, TetMesh::csg_difference, &vis_data));

			for(auto pair : vis_data.poly_soups)
				Mesh::makePolySoup(pair.second).draw(out, makeMat(pair.first, false), matrix);
			for(auto pair : vis_data.segment_groups)
				out.addSegments(pair.second, makeMat(pair.first, false), matrix);
			for(auto pair : vis_data.segment_groups_trans)
				out.addSegments(pair.second, makeMat(pair.first, true), matrix);
			for(auto pair : vis_data.tet_meshes)
				drawTets(*PTetMesh(pair.second), out, matrix, pair.first);
			for(auto pair : vis_data.point_sets) {
				auto mat = makeMat(Color(pair.first, min((int)pair.first.a, 254)), false);
				auto box = Mesh::makeBBox(FBox(-1, -1, -1, 1, 1, 1) * scale * 0.03f);
				for(auto point : pair.second)
					box.draw(out, mat, matrix * translation(point));
			}
		}

		void drawCsg(Renderer &out, const Matrix4 &matrix, Color color, float3 offset, float scale,
					 int num_steps, int csg_phase, int csg_mesh_id) {
			PMaterial tri_mat = Material(Color::red);
			PMaterial line_mat = Material(Color::black, Material::flag_ignore_depth);
			PMaterial line2_mat = Material(Color::yellow, Material::flag_ignore_depth);

			auto mesh_a = DynamicMesh(m_model->toMesh());
			auto mesh_b = DynamicMesh(Mesh::transform(translation(offset), Mesh(mesh_a)));
			if(csg_mesh_id)
				swap(mesh_a, mesh_b);

			vector<Segment> segments;
			vector<Triangle> tris;
			vector<vector<Triangle>> segs;

			DynamicMesh::CSGVisualData vis_data;
			vis_data.max_steps = num_steps;
			vis_data.phase = csg_phase;

			auto csg = DynamicMesh::csgDifference(mesh_a, mesh_b, &vis_data);

			for(auto pair : vis_data.poly_soups)
				Mesh::makePolySoup(pair.second).draw(out, makeMat(pair.first, false), matrix);
			for(auto pair : vis_data.segment_groups)
				out.addSegments(pair.second, makeMat(pair.first, false), matrix);
			for(auto pair : vis_data.segment_groups_trans)
				out.addSegments(pair.second, makeMat(pair.first, true), matrix);
			for(auto pair : vis_data.point_sets) {
				auto mat = makeMat(Color(pair.first, min((int)pair.first.a, 254)), false);
				auto box = Mesh::makeBBox(FBox(-1, -1, -1, 1, 1, 1) * scale * 0.03f);
				for(auto point : pair.second)
					box.draw(out, mat, matrix * translation(point));
			}
		}

		PModel m_model;
		PTetMesh m_tet_mesh;
		PMesh m_tet_isects;
		MaterialSet m_materials;
		string m_model_name;
		string m_tex_name;
		int m_num_segments;
		int m_num_non_manifold;
		bool m_tets_computed;
	};

	void makeMaterials(PMaterial default_mat, Model &model) {}

	void updateViewport() {
		IRect new_viewport = IRect(GfxDevice::instance().windowSize());
		if(new_viewport != m_viewport || !m_renderer_3d) {
			m_viewport = new_viewport;
			m_renderer_3d = make_unique<Renderer>(m_viewport);
			m_renderer_2d = make_unique<Renderer2D>(m_viewport);
		}
	}

	using Mode = ViewerMode::Type;

	Viewer(const vector<pair<string, string>> &file_names)
		: m_current_model(0), m_current_anim(-1), m_anim_pos(0.0), m_show_nodes(false),
		  m_mode(Mode::model_csg), m_num_steps(0), m_csg_mesh_id(0), m_csg_phase(0) {
		updateViewport();

		m_csg_offset = float3(0.1, 0, 0.3);
		if(access("tet_offset.data")) {
			Loader file("tet_offset.data");
			auto range = Range<float>(m_csg_offset.v);
			file.loadData(range.data(), range.size() * sizeof(float));
		}

		for(auto file_name : file_names) {
			PTexture tex;
			if(!file_name.second.empty() && access(file_name.second)) {
				double time = getTime();
				tex = s_textures[file_name.second];
				printf("Loaded texture %s: %.2f ms\n", file_name.second.c_str(),
					   (getTime() - time) * 1000.0f);
			}

			PMaterial default_mat = make_immutable<Material>(tex);

			double time = getTime();
			auto model = s_models[file_name.first];
			printf("Loaded %s: %.2f ms\n", file_name.first.c_str(), (getTime() - time) * 1000.0f);
			m_models.emplace_back(model, default_mat, tex, file_name.first, file_name.second);
		}

		FontFactory factory;
		m_font_data = factory.makeFont("data/LiberationSans-Regular.ttf", 14, false);

		if(m_models.empty())
			THROW("No models loaded\n");
	}

	void handleInput(GfxDevice &device, float time_diff) {
		float x_rot = 0.0f, y_rot = 0.0f;
		float scale = 0.0f;

		for(const auto &event : device.inputEvents()) {
			bool shift = event.hasModifier(InputEvent::mod_lshift);

			if(event.keyDown('q'))
				m_mode = (Mode)((m_mode + 1) % ViewerMode::count);
			if(event.hasModifier(InputEvent::mod_lctrl) &&
			   isOneOf(m_mode, Mode::tets_csg, Mode::model_csg)) {
				if(event.keyPressed(InputKey::left))
					m_csg_offset.x -= time_diff;
				if(event.keyPressed(InputKey::right))
					m_csg_offset.x += time_diff;
				if(event.keyPressed(InputKey::up))
					m_csg_offset.z -= time_diff;
				if(event.keyPressed(InputKey::down))
					m_csg_offset.z += time_diff;

			} else {
				if(event.keyPressed(InputKey::left))
					x_rot -= time_diff * 2.0f;
				if(event.keyPressed(InputKey::right))
					x_rot += time_diff * 2.0f;
				if(event.keyPressed(InputKey::up))
					y_rot -= time_diff * 2.0f;
				if(event.keyPressed(InputKey::down))
					y_rot += time_diff * 2.0f;
			}
			if(event.keyPressed(InputKey::pageup))
				scale += time_diff * 2.0f;
			if(event.keyPressed(InputKey::pagedown))
				scale -= time_diff * 2.0f;
			if(event.keyDown('m')) {
				m_current_model =
					(m_current_model + (shift ? m_models.size() - 1 : 1)) % m_models.size();
				m_current_anim = -1;
				m_anim_pos = 0.0;
			}
			if(event.keyDown('a')) {
				m_current_anim++;
				if(m_current_anim == m_models[m_current_model].m_model->animCount())
					m_current_anim = -1;
				m_anim_pos = 0.0f;
			}
			if(event.keyDown('s'))
				m_show_nodes ^= 1;
			if(event.keyDown('i') && m_num_steps > 0)
				m_num_steps = max(0, m_num_steps - (shift ? 10 : 1));
			if(event.keyDown('k'))
				m_num_steps += shift ? 10 : 1;
			if(event.keyDown('l')) {
				int off = shift ? TetMesh::CSGVisualData::max_phases - 1 : 1;
				m_csg_phase = (m_csg_phase + off) % TetMesh::CSGVisualData::max_phases;
			}
			if(event.keyDown('o'))
				m_csg_mesh_id = (m_csg_mesh_id + 1) % 2;
			if(event.keyDown('p')) {
				Saver file("tet_offset.data");
				auto range = Range<float>(m_csg_offset.v);
				file.saveData(range.data(), range.size() * sizeof(float));
			}
		}

		Quat rot = normalize(Quat(AxisAngle({0, 1, 0}, x_rot)) * Quat(AxisAngle({1, 0, 0}, y_rot)));

		m_target_view.zoom = clamp(m_target_view.zoom * (1.0f + scale), 0.2f, 4.0f);
		m_target_view.rot = normalize(rot * m_target_view.rot);
	}

	void tick(float time_diff) {
		m_view_config = lerp(m_view_config, m_target_view, 0.1f);
		m_anim_pos += time_diff;
	}

	static void faceVertHistogram(TextFormatter &out, PModel model) {
		DynamicMesh dmesh(model->toMesh());
		std::map<int, int> fcounts;

		for(auto vert : dmesh.verts())
			fcounts[dmesh.polyCount(vert)]++;
		out("Faces/vert: ");
		for(auto it : fcounts)
			out("%d:%d ", it.first, it.second);
		out("\n");
	}

	void draw() {
		m_renderer_3d->setProjectionMatrix(perspective(
			degToRad(60.0f), float(m_viewport.width()) / m_viewport.height(), 1.0f, 10000.0f));
		m_renderer_3d->setViewMatrix(translation(0, 0, -5.0f));

		bool show_tets = isOneOf(m_mode, Mode::tets, Mode::tets_csg);
		auto &model = m_models[m_current_model];
		if(show_tets)
			model.updateTets();

		auto pose = m_mode == Mode::model ? model.animatePose(m_current_anim, m_anim_pos) : PPose();
		auto matrix = scaling(m_view_config.zoom * model.scale()) * Matrix4(m_view_config.rot) *
					  translation(-model.boundingBox(pose).center());

		if(m_mode == Mode::model) {
			model.drawModel(*m_renderer_3d, pose, m_show_nodes, matrix);
			m_renderer_3d->addWireBox(model.boundingBox(pose), {Color::green}, matrix);
		} else if(m_mode == Mode::tets)
			model.drawTets(*m_renderer_3d, matrix, Color(80, 255, 200));
		else if(m_mode == Mode::tets_csg)
			model.drawTetsCsg(*m_renderer_3d, matrix, Color(80, 255, 200), m_csg_offset,
							  1.0f / model.scale(), m_num_steps, m_csg_phase, m_csg_mesh_id);
		else if(m_mode == Mode::model_csg)
			model.drawCsg(*m_renderer_3d, matrix, Color(80, 255, 200), m_csg_offset,
						  1.0f / model.scale(), m_num_steps, m_csg_phase, m_csg_mesh_id);

		TextFormatter fmt;
		fmt("Mode: %s (Q)\n", toString(m_mode));
		fmt("Model: %s (%d / %d)\n", model.m_model_name.c_str(), m_current_model + 1,
			(int)m_models.size());
		// fmt("Texture: %s\n", !model.m_tex_name.empty() ? model.m_tex_name.c_str() : "none");
		string anim_name =
			m_current_anim == -1 ? "none" : model.m_model->anim(m_current_anim).name();
		fmt("Animation: %s (%d / %d)\n", anim_name.c_str(), m_current_anim + 1,
			(int)model.m_model->animCount());
		fmt("Help:\n");
		fmt("M: change model\n");
		fmt("A: change animation\n");
		fmt("S: display skeleton\n");
		fmt("up/down/left/right: rotate\n");
		fmt("pgup/pgdn: zoom\n\n");
		if(isOneOf(m_mode, Mode::tets_csg, Mode::model_csg)) {
			fmt("ctrl+ up/down/left/right: move mesh\n");
			fmt("phase: %d steps: %d\n", m_csg_phase, m_num_steps);
		}
		// faceVertHistogram(fmt, model.m_model);

		model.printModelStats(fmt);
		if(show_tets)
			model.printTetStats(fmt);
		fmt("%s", Profiler::instance()->getStats("X").c_str());

		FontRenderer font(m_font_data.first, m_font_data.second, *m_renderer_2d);
		FontStyle style{Color::white, Color::black};
		auto extents = font.evalExtents(fmt.text());
		m_renderer_2d->addFilledRect(FRect(float2(extents.size()) + float2(10, 10)),
									 {Color(0, 0, 0, 80)});
		font.draw(FRect(5, 5, 300, 100), style, fmt.text());

		m_renderer_3d->render();
		m_renderer_2d->render();
	}

	const IRect &viewport() const { return m_viewport; }

  private:
	vector<Model> m_models;
	pair<PFont, PTexture> m_font_data;
	PProgram m_flat_program;

	IRect m_viewport;
	int m_current_model, m_current_anim;
	double m_anim_pos;
	bool m_show_nodes;
	ViewConfig m_view_config;
	ViewConfig m_target_view;

	float3 m_csg_offset;

	unique_ptr<Renderer> m_renderer_3d;
	unique_ptr<Renderer2D> m_renderer_2d;
	Mode m_mode;
	int m_num_steps, m_csg_mesh_id, m_csg_phase;
};

static Viewer *s_viewer = nullptr;

bool main_loop(GfxDevice &device) {
	DASSERT(s_viewer);

	Color nice_background(200, 200, 255);
	GfxDevice::clearColor(nice_background);
	GfxDevice::clearDepth(1.0f);

	float time_diff = 1.0f / 60.0f;
	s_viewer->handleInput(device, time_diff);
	s_viewer->tick(time_diff);
	s_viewer->updateViewport();
	s_viewer->draw();

	auto *profiler = Profiler::instance();
	profiler->nextFrame();

	return true;
}

int safe_main(int argc, char **argv) {
	double time = getTime();
	int2 resolution(1200, 700);

	if(argc <= 1) {
		printf("Usage:\n%s model_name.model\n%s data/*.model\n", argv[0], argv[0]);
		return 0;
	}

	string model_argument = argv[1], tex_argument = argc > 2 ? argv[2] : "";
	vector<pair<string, string>> files;

	if(model_argument.find('*') != string::npos) {
		auto star_pos = model_argument.find('*');
		string prefix = model_argument.substr(0, star_pos);
		string suffix = model_argument.substr(star_pos + 1);

		auto tstar_pos = tex_argument.find('*');
		string tprefix = star_pos == string::npos ? "" : tex_argument.substr(0, tstar_pos);
		string tsuffix = star_pos == string::npos ? "" : tex_argument.substr(tstar_pos + 1);

		FilePath src_folder(prefix);
		while(!src_folder.isDirectory())
			src_folder = src_folder.parent();
		auto found_files = findFiles(src_folder, FindFiles::regular_file | FindFiles::recursive);

		for(const auto &file : found_files) {
			string name = file.path;
			if(removePrefix(name, prefix) && removeSuffix(name, suffix)) {
				string tex_name =
					tstar_pos == string::npos ? tex_argument : tprefix + name + tsuffix;
				files.emplace_back(file.path, tex_name);
			}
		}
	} else {
		files = {make_pair(model_argument, tex_argument)};
	}

	Profiler profiler;
	GfxDevice gfx_device;
	uint flags = GfxDevice::flag_resizable | GfxDevice::flag_multisampling | GfxDevice::flag_vsync;
	gfx_device.createWindow("libfwk::model_viewer", resolution, flags);

	double init_time = getTime();
	Viewer viewer(files);
	s_viewer = &viewer;

	gfx_device.runMainLoop(main_loop);

	return 0;
}

int main(int argc, char **argv) {
	try {
		return safe_main(argc, argv);
	} catch(const Exception &ex) {
		printf("%s\n\nBacktrace:\n%s\n", ex.what(), ex.backtrace().c_str());
		return 1;
	}
}
