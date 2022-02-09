// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/animated_model.h"
#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/dynamic_mesh.h"
#include "fwk/gfx/font_factory.h"
#include "fwk/gfx/font_finder.h"
#include "fwk/gfx/gl_device.h"
#include "fwk/gfx/gl_texture.h"
#include "fwk/gfx/line_buffer.h"
#include "fwk/gfx/material_set.h"
#include "fwk/gfx/mesh.h"
#include "fwk/gfx/model.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/render_list.h"
#include "fwk/gfx/renderer2d.h"
#include "fwk/gfx/triangle_buffer.h"
#include "fwk/io/file_system.h"
#include "fwk/math/axis_angle.h"
#include "fwk/math/rotation.h"
#include "fwk/sys/input.h"

#include "fwk/menu/imgui_wrapper.h"

#ifndef FWK_IMGUI_DISABLED
#include "fwk/menu/helpers.h"
#include "fwk/menu_imgui.h"
#endif

using namespace fwk;

namespace {
HashMap<string, PTexture> s_textures;

string dataPath(string file_name) {
#ifdef FWK_PLATFORM_MSVC
	auto main_path = FilePath::current().get();
#else
	auto main_path = FilePath(executablePath()).parent().parent();
#endif
	return main_path / "data" / file_name;
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

struct ViewModel {
	ViewModel(Model model, const Material &default_mat, PTexture tex, string mname, string tname)
		: m_model(std::move(model)), m_materials(default_mat), m_model_name(mname),
		  m_tex_name(tname), m_num_segments(0) {
		HashMap<string, Material> map;

		for(const auto &def : m_model.materialDefs()) {
			map[def.name] =
				tex ? Material({tex}, IColor(def.diffuse)) : Material(IColor(def.diffuse));
		}
		m_materials = MaterialSet(default_mat, map);
		map.clear();
	}

	Pose animatePose(int anim_id, float anim_pos) const {
		return m_model.animatePose(anim_id, anim_pos);
	}

	auto animate(const Pose &pose) const { return AnimatedModel(m_model, pose); }
	FBox boundingBox(const Pose &pose) const { return animate(pose).boundingBox(); }
	FBox boundingBox() const { return boundingBox(m_model.defaultPose()); }

	float scale() const { return 4.0f / max(boundingBox().size().values()); }

	void drawModel(RenderList &out, const Pose &pose, const Matrix4 &matrix) {
		out.add(animate(pose).genDrawCalls(m_materials, matrix));
	}

	void drawNodes(TriangleBuffer &tris, LineBuffer &lines, const Pose &pose) {
		m_model.drawNodes(tris, lines, pose, ColorId::green, ColorId::yellow, 0.1f / scale());
	}

	void printModelStats(TextFormatter &fmt) const {
		int num_parts = 0, num_verts = 0, num_faces = 0;
		for(const auto &node : m_model.nodes())
			if(auto *mesh = m_model.mesh(node.mesh_id)) {
				num_parts++;
				num_verts += mesh->vertexCount();
				num_faces += mesh->triangleCount();
			}
		FBox bbox = boundingBox();
		fmt("Size: %\n\n", FormatMode::structured, bbox.size());
		fmt("Parts: %  Verts: % Faces: %\n", num_parts, num_verts, num_faces);
	}

	Material makeMat(IColor col, bool line) {
		auto flags = col.a != 255 || line ? MaterialOpt::blended | MaterialOpt::ignore_depth : none;
		return Material{col, flags};
	}

	Model m_model;
	MaterialSet m_materials;
	string m_model_name;
	string m_tex_name;
	int m_num_segments;
};

class Viewer {
  public:
	void makeMaterials(Material default_mat, ViewModel &model) {}

	void updateViewport() { m_viewport = IRect(GlDevice::instance().windowSize()); }

	Viewer(const vector<Pair<string>> &file_names)
		: m_current_model(0), m_current_anim(-1), m_anim_pos(0.0), m_show_nodes(false) {
		updateViewport();

		for(auto file_name : file_names) {
			PTexture tex;
			if(!file_name.second.empty() && access(file_name.second)) {
				double time = getTime();
				if(auto it = s_textures.find(file_name.second))
					tex = it->value;
				else {
					tex = GlTexture::load(file_name.second, true).get();
					s_textures[file_name.second] = tex;
				}

				printf("Loaded texture %s: %.2f ms\n", file_name.second.c_str(),
					   (getTime() - time) * 1000.0f);
			}

			Material default_mat = tex ? Material({tex}) : Material();

			double time = getTime();
			auto model = Model::load(file_name.first);

			if(model) {
				printf("Loaded %s: %.2f ms\n", file_name.first.c_str(),
					   (getTime() - time) * 1000.0f);
				m_models.emplace_back(move(model.get()), default_mat, tex, file_name.first,
									  file_name.second);
			} else {
				print("Error while loading '%':\n%", file_name.first, model.error());
			}
		}

#ifndef FWK_IMGUI_DISABLED
		m_imgui.emplace(GlDevice::instance(), ImGuiOptions{none, none, ImGuiStyleMode::mini});
#endif

		auto font_path = findDefaultSystemFont().get();
		m_font.emplace(move(FontFactory().makeFont(font_path, 14, false).get()));
	}

	string helpText() const {
		TextFormatter fmt;
		fmt("Help:\n");
		fmt("M/shift + M: change model\n");
		fmt("A/shift + A: change animation\n");
		fmt("S: display skeleton\n");
		fmt("up/down/left/right: rotate\n");
		fmt("pgup/pgdn: zoom\n\n");
		return fmt.text();
	}

	void helpBox(Renderer2D &renderer_2d, ViewModel &model) const {
		TextFormatter fmt;
		fmt("[Imgui disabled]\n");
		fmt("Model: % (% / %)\n", model.m_model_name, m_current_model + 1, m_models.size());
		// fmt("Texture: %s\n", !model.m_tex_name.empty() ? model.m_tex_name : "none");
		string anim_name =
			m_current_anim == -1 ? "none" : model.m_model.anim(m_current_anim).name();
		fmt("Animation: % (% / %)\n", anim_name, m_current_anim + 1, model.m_model.animCount());

		fmt << helpText();
		model.printModelStats(fmt);

		FontStyle style{ColorId::white, ColorId::black};
		auto extents = m_font->evalExtents(fmt.text());
		renderer_2d.addFilledRect(FRect(float2(extents.size()) + float2(10, 10)),
								  {IColor(0, 0, 0, 80)});
		m_font->draw(renderer_2d, FRect({5, 5}, {300, 100}), style, fmt.text());
	}

	void doMenu() {
#ifndef FWK_IMGUI_DISABLED
		static bool set_pos = true;
		if(set_pos) {
			menu::SetNextWindowPos(int2());
			menu::SetNextWindowSize({350, 300});
			set_pos = false;
		}
		menu::Begin("Control", nullptr,
					ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		menu::text("Model:");
		menu::SameLine();
		if(menu::BeginCombo("##model", m_models[m_current_model].m_model_name.c_str())) {
			for(int n : intRange(m_models))
				if(menu::Selectable(m_models[n].m_model_name.c_str(), n == m_current_model))
					m_current_model = n;
			menu::EndCombo();
		}

		auto &model = m_models[m_current_model];
		auto &anims = model.m_model.anims();
		menu::text("Animation:");
		menu::SameLine();
		const char *cur_anim =
			!anims.inRange(m_current_anim) ? "empty" : anims[m_current_anim].name().c_str();
		if(menu::BeginCombo("##anim", cur_anim)) {
			if(menu::Selectable("empty", m_current_anim == -1))
				m_current_anim = -1;
			for(int n : intRange(anims))
				if(menu::Selectable(anims[n].name().c_str(), m_current_anim == n))
					m_current_anim = n;
			menu::EndCombo();
		}

		menu::Separator();
		menu::text(helpText());
		menu::End();
#endif
	}

	bool handleInput(vector<InputEvent> events, float time_diff) {
		float x_rot = 0.0f, y_rot = 0.0f;
		float scale = 0.0f;

		for(const auto &event : events) {
			bool shift = event.pressed(InputModifier::lshift);

			if(event.keyPressed(InputKey::left))
				x_rot -= time_diff * 2.0f;
			if(event.keyPressed(InputKey::right))
				x_rot += time_diff * 2.0f;
			if(event.keyPressed(InputKey::up))
				y_rot -= time_diff * 2.0f;
			if(event.keyPressed(InputKey::down))
				y_rot += time_diff * 2.0f;
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
				if(m_current_anim == m_models[m_current_model].m_model.animCount())
					m_current_anim = -1;
				m_anim_pos = 0.0f;
			}
			if(event.keyDown('s'))
				m_show_nodes ^= 1;
			if(event.keyDown(InputKey::esc))
				return false;
		}

		Quat rot = normalize(Quat(AxisAngle({0, 1, 0}, x_rot)) * Quat(AxisAngle({1, 0, 0}, y_rot)));

		m_target_view.zoom = clamp(m_target_view.zoom * (1.0f + scale), 0.2f, 4.0f);
		m_target_view.rot = normalize(rot * m_target_view.rot);
		return true;
	}

	bool valid() const { return !!m_models; }

	void tick(float time_diff) {
		m_view_config = lerp(m_view_config, m_target_view, 0.1f);
		m_anim_pos += time_diff;
	}

	static void faceVertHistogram(TextFormatter &out, const Model &model) {
		DynamicMesh dmesh(AnimatedModel(model).toMesh());
		HashMap<int, int> fcounts;

		for(auto vert : dmesh.verts())
			fcounts[dmesh.polyCount(vert)]++;
		out("Faces/vert: ");
		for(auto it : fcounts)
			out("%:% ", it.key, it.value);
		out("\n");
	}

	void draw() {
		Matrix4 proj = perspective(degToRad(60.0f), float(m_viewport.width()) / m_viewport.height(),
								   1.0f, 10000.0f);
		RenderList renderer_3d(m_viewport, proj);

		renderer_3d.setViewMatrix(translation(0, 0, -5.0f));

		auto &model = m_models[m_current_model];

		auto anim_id = model.m_model.anims().inRange(m_current_anim) ? m_current_anim : -1;
		auto pose = model.animatePose(anim_id, m_anim_pos);
		auto matrix = scaling(m_view_config.zoom * model.scale()) * Matrix4(m_view_config.rot) *
					  translation(-model.boundingBox().center());

		TriangleBuffer tris;
		LineBuffer lines;
		tris.setTrans(matrix);
		lines.setTrans(matrix);

		model.drawModel(renderer_3d, pose, matrix);
		if(m_show_nodes)
			model.drawNodes(tris, lines, pose);
		lines(model.boundingBox(pose), ColorId::green);

		renderer_3d.add(tris.drawCalls());
		renderer_3d.add(lines.drawCalls());

		renderer_3d.render();

#ifdef FWK_IMGUI_DISABLED
		Renderer2D renderer_2d(m_viewport, Orient2D::y_down);
		helpBox(renderer_2d, model);
		renderer_2d.render();
#else
		m_imgui->drawFrame(GlDevice::instance());
#endif
	}

	const IRect &viewport() const { return m_viewport; }

	bool mainLoop(GlDevice &device) {
		IColor nice_background(200, 200, 255);
		clearColor(nice_background);
		clearDepth(1.0f);

		vector<InputEvent> events;
#ifdef FWK_IMGUI_DISABLED
		events = device.inputEvents();
#else
		m_imgui->beginFrame(device);
		doMenu();
		events = m_imgui->finishFrame(device);
#endif

		float time_diff = 1.0f / 60.0f;
		if(!handleInput(events, time_diff))
			return false;
		tick(time_diff);
		updateViewport();
		draw();

		return true;
	}

	static bool mainLoop(GlDevice &device, void *this_ptr) {
		return ((Viewer *)this_ptr)->mainLoop(device);
	}

  private:
	vector<ViewModel> m_models;
	Dynamic<Font> m_font;
#ifndef FWK_IMGUI_DISABLED
	Dynamic<ImGuiWrapper> m_imgui;
#endif

	IRect m_viewport;
	int m_current_model, m_current_anim;
	double m_anim_pos;
	bool m_show_nodes;
	ViewConfig m_view_config;
	ViewConfig m_target_view;
};

int main(int argc, char **argv) {
	double time = getTime();
	int2 resolution(1200, 700);

	string models_path = "./", tex_argument = "";
	if(argc <= 1) {
		printf("Usage:\n%s model_name.model\n%s data/*.model\n", argv[0], argv[0]);
		printf("Loading models from current directory (recursively)\n");
	} else {
		models_path = argv[1];
		tex_argument = argc > 2 ? argv[2] : "";
	}

	vector<Pair<string>> files;

	bool multiple_files = models_path.find('*') != string::npos;
	if(!multiple_files) {
		if(FilePath(models_path).isDirectory()) {
			models_path += "*.model";
			multiple_files = true;
		} else {
			files = {{models_path, tex_argument}};
		}
	}

	if(multiple_files) {
		auto star_pos = models_path.find('*');
		string prefix = models_path.substr(0, star_pos);
		string suffix = models_path.substr(star_pos + 1);

		auto tstar_pos = tex_argument.find('*');
		string tprefix = star_pos == string::npos ? "" : tex_argument.substr(0, tstar_pos);
		string tsuffix = star_pos == string::npos ? "" : tex_argument.substr(tstar_pos + 1);

		FilePath src_folder(prefix);
		while(!src_folder.isDirectory())
			src_folder = src_folder.parent();

		auto opts = FindFileOpt::regular_file | FindFileOpt::recursive;
		auto found_files = findFiles(src_folder, opts);

		for(const auto &file : found_files) {
			string name = file.path;

			if((prefix == "./" || removePrefix(name, prefix)) && removeSuffix(name, suffix)) {
				string tex_name =
					tstar_pos == string::npos ? tex_argument : tprefix + name + tsuffix;
				files.emplace_back(file.path, tex_name);
			}
		}
	}

	GlDevice gfx_device;
	GlDeviceConfig gl_config;
	gl_config.flags = GlDeviceOpt::resizable | GlDeviceOpt::vsync;
	gl_config.multisampling = 4;
	gfx_device.createWindow("libfwk::model_viewer", resolution, gl_config);

	double init_time = getTime();
	Viewer viewer(files);
	if(!viewer.valid()) {
		print("No models\n");
		return 0;
	}

	gfx_device.runMainLoop(Viewer::mainLoop, &viewer);

	return 0;
}
