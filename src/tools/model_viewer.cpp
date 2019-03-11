// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/filesystem.h"
#include "fwk/gfx/animated_model.h"
#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/dynamic_mesh.h"
#include "fwk/gfx/font_factory.h"
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
#include "fwk/math/axis_angle.h"
#include "fwk/math/rotation.h"
#include "fwk/sys/input.h"
#include "fwk/sys/xml_loader.h"
#include "fwk_profile.h"

using namespace fwk;

namespace {
ResourceManager<Model, XmlLoader<Model>> s_models("", "", "");
HashMap<string, PTexture> s_textures;

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

class Viewer {
  public:
	struct Model {
		Model(PModel model, const Material &default_mat, PTexture tex, string mname, string tname)
			: m_model(std::move(model)), m_materials(default_mat), m_model_name(mname),
			  m_tex_name(tname), m_num_segments(0) {
			HashMap<string, Material> map;

			for(const auto &def : m_model->materialDefs()) {
				map[def.name] =
					tex ? Material({tex}, IColor(def.diffuse)) : Material(IColor(def.diffuse));
			}
			m_materials = MaterialSet(default_mat, map);
			map.clear();
		}

		PPose animatePose(int anim_id, float anim_pos) const {
			return m_model->animatePose(anim_id, anim_pos);
		}

		auto animate(PPose pose) const { return AnimatedModel(*m_model, pose); }
		FBox boundingBox(PPose pose = PPose()) const { return animate(pose).boundingBox(); }

		float scale() const { return 4.0f / max(boundingBox().size().values()); }

		void drawModel(RenderList &out, PPose pose, const Matrix4 &matrix) {
			out.add(animate(pose).genDrawCalls(m_materials, matrix));
		}

		void drawNodes(TriangleBuffer &tris, LineBuffer &lines, PPose pose) {
			m_model->drawNodes(tris, lines, pose, ColorId::green, ColorId::yellow, 0.1f / scale());
		}

		void printModelStats(TextFormatter &fmt) const {
			int num_parts = 0, num_verts = 0, num_faces = 0;
			for(const auto &node : m_model->nodes())
				if(node->mesh()) {
					num_parts++;
					num_verts += node->mesh()->vertexCount();
					num_faces += node->mesh()->triangleCount();
				}
			FBox bbox = boundingBox();
			fmt("Size: %\n\n", FormatMode::structured, bbox.size());
			fmt("Parts: %  Verts: % Faces: %\n", num_parts, num_verts, num_faces);
		}

		Material makeMat(IColor col, bool line) {
			auto flags =
				col.a != 255 || line ? MaterialOpt::blended | MaterialOpt::ignore_depth : none;
			return Material{col, flags};
		}

		PModel m_model;
		MaterialSet m_materials;
		string m_model_name;
		string m_tex_name;
		int m_num_segments;
	};

	void makeMaterials(Material default_mat, Model &model) {}

	void updateViewport() { m_viewport = IRect(GlDevice::instance().windowSize()); }

	Viewer(const vector<Pair<string>> &file_names)
		: m_current_model(0), m_current_anim(-1), m_anim_pos(0.0), m_show_nodes(false) {
		updateViewport();

		for(auto file_name : file_names) {
			PTexture tex;
			if(!file_name.second.empty() && access(file_name.second)) {
				double time = getTime();
				auto it = s_textures.find(file_name.second);
				if(it == s_textures.end()) {
					Loader ldr(file_name.second);
					tex.emplace(file_name.second, ldr);
					s_textures[file_name.second] = tex;
				} else {
					tex = it->second;
				}
				printf("Loaded texture %s: %.2f ms\n", file_name.second.c_str(),
					   (getTime() - time) * 1000.0f);
			}

			Material default_mat = tex ? Material({tex}) : Material();

			double time = getTime();
			auto model = s_models[file_name.first];
			printf("Loaded %s: %.2f ms\n", file_name.first.c_str(), (getTime() - time) * 1000.0f);
			m_models.emplace_back(model, default_mat, tex, file_name.first, file_name.second);
		}

		FontFactory factory;
		auto font_path = dataPath("LiberationSans-Regular.ttf");
		m_font = factory.makeFont(font_path, 14, false);

		if(m_models.empty())
			CHECK_FAILED("No models loaded\n");
	}

	bool handleInput(GlDevice &device, float time_diff) {
		float x_rot = 0.0f, y_rot = 0.0f;
		float scale = 0.0f;

		for(const auto &event : device.inputEvents()) {
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
				if(m_current_anim == m_models[m_current_model].m_model->animCount())
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

	void tick(float time_diff) {
		m_view_config = lerp(m_view_config, m_target_view, 0.1f);
		m_anim_pos += time_diff;
	}

	static void faceVertHistogram(TextFormatter &out, PModel model) {
		DynamicMesh dmesh(AnimatedModel(*model).toMesh());
		std::map<int, int> fcounts;

		for(auto vert : dmesh.verts())
			fcounts[dmesh.polyCount(vert)]++;
		out("Faces/vert: ");
		for(auto it : fcounts)
			out("%:% ", it.first, it.second);
		out("\n");
	}

	void draw() {
		Matrix4 proj = perspective(degToRad(60.0f), float(m_viewport.width()) / m_viewport.height(),
								   1.0f, 10000.0f);
		RenderList renderer_3d(m_viewport, proj);
		Renderer2D renderer_2d(m_viewport, Orient2D::y_down);

		renderer_3d.setViewMatrix(translation(0, 0, -5.0f));

		auto &model = m_models[m_current_model];

		auto pose = model.animatePose(m_current_anim, m_anim_pos);
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

		TextFormatter fmt;
		fmt("Model: % (% / %)\n", model.m_model_name, m_current_model + 1, m_models.size());
		// fmt("Texture: %s\n", !model.m_tex_name.empty() ? model.m_tex_name : "none");
		string anim_name =
			m_current_anim == -1 ? "none" : model.m_model->anim(m_current_anim).name();
		fmt("Animation: % (% / %)\n", anim_name, m_current_anim + 1, model.m_model->animCount());
		fmt("Help:\n");
		fmt("M: change model\n");
		fmt("A: change animation\n");
		fmt("S: display skeleton\n");
		fmt("up/down/left/right: rotate\n");
		fmt("pgup/pgdn: zoom\n\n");

		model.printModelStats(fmt);
		fmt("%", Profiler::instance()->getStats("X"));

		FontStyle style{ColorId::white, ColorId::black};
		auto extents = m_font->evalExtents(fmt.text());
		renderer_2d.addFilledRect(FRect(float2(extents.size()) + float2(10, 10)),
								  {IColor(0, 0, 0, 80)});
		m_font->draw(renderer_2d, FRect({5, 5}, {300, 100}), style, fmt.text());

		renderer_3d.add(tris.drawCalls());
		renderer_3d.add(lines.drawCalls());

		renderer_3d.render();
		renderer_2d.render();
	}

	const IRect &viewport() const { return m_viewport; }

	bool mainLoop(GlDevice &device) {
		IColor nice_background(200, 200, 255);
		clearColor(nice_background);
		clearDepth(1.0f);

		float time_diff = 1.0f / 60.0f;
		if(!handleInput(device, time_diff))
			return false;
		tick(time_diff);
		updateViewport();
		draw();

		auto *profiler = Profiler::instance();
		profiler->nextFrame();

		return true;
	}

	static bool mainLoop(GlDevice &device, void *this_ptr) {
		return ((Viewer *)this_ptr)->mainLoop(device);
	}

  private:
	vector<Model> m_models;
	UniquePtr<Font> m_font;

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

	if(argc <= 1) {
		printf("Usage:\n%s model_name.model\n%s data/*.model\n", argv[0], argv[0]);
		return 0;
	}

	string model_argument = argv[1], tex_argument = argc > 2 ? argv[2] : "";
	vector<Pair<string>> files;

	bool multiple_files = model_argument.find('*') != string::npos;
	if(!multiple_files) {
		if(FilePath(model_argument).isDirectory()) {
			model_argument += "*.model";
			multiple_files = true;
		} else {
			files = {{model_argument, tex_argument}};
		}
	}

	if(multiple_files) {
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
	}

	Profiler profiler;
	GlDevice gfx_device;
	auto flags = GlDeviceOpt::resizable | GlDeviceOpt::multisampling | GlDeviceOpt::vsync;
	gfx_device.createWindow("libfwk::model_viewer", resolution, flags);

	double init_time = getTime();
	Viewer viewer(files);
	gfx_device.runMainLoop(Viewer::mainLoop, &viewer);

	return 0;
}
