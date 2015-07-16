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

class Viewer {
  public:
	struct Model {
		PModel model;
		PTexture tex;
		string model_name;
		string tex_name;
	};

	Viewer(const IRect &viewport, const vector<pair<string, string>> &file_names)
		: m_viewport(viewport), m_current_model(0), m_current_anim(-1), m_anim_pos(0.0),
		  m_show_nodes(false) {
		for(auto file_name : file_names) {
			PTexture tex;
			if(!file_name.second.empty() && access(file_name.second)) {
				double time = getTime();
				tex = s_textures[file_name.second];
				printf("Loaded texture %s: %.2f ms\n", file_name.second.c_str(),
					   (getTime() - time) * 1000.0f);
			}

			double time = getTime();
			auto model = s_models[file_name.first];
			printf("Loaded %s: %.2f ms\n", file_name.first.c_str(), (getTime() - time) * 1000.0f);
			m_models.emplace_back(Model{model, tex, file_name.first, file_name.second});
		}

		FontFactory factory;
		m_font_data = factory.makeFont("data/LiberationSans-Regular.ttf", 14, false);

		if(m_models.empty())
			THROW("No models loaded\n");

		Shader vertex_shader(Shader::tVertex), fragment_shader(Shader::tFragment);
		Loader(dataPath("flat_shader.vsh")) >> vertex_shader;
		Loader(dataPath("flat_shader.fsh")) >> fragment_shader;
		m_flat_program = make_shared<Program>(vertex_shader, fragment_shader);
		m_flat_program->link();
	}

	void handleInput(GfxDevice &device, float time_diff) {
		float x_rot = 0.0f, y_rot = 0.0f;
		float scale = 0.0f;

		for(const auto &event : device.inputEvents()) {
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
				m_current_model = (m_current_model + 1) % m_models.size();
				m_current_anim = -1;
				m_anim_pos = 0.0;
			}
			if(event.keyDown('a')) {
				m_current_anim++;
				if(m_current_anim == m_models[m_current_model].model->animCount())
					m_current_anim = -1;
				m_anim_pos = 0.0f;
			}
			if(event.keyDown('s'))
				m_show_nodes ^= 1;
		}

		Quat rot = normalize(Quat(AxisAngle({0, 1, 0}, x_rot)) * Quat(AxisAngle({1, 0, 0}, y_rot)));

		m_target_view.zoom = clamp(m_target_view.zoom * (1.0f + scale), 0.2f, 4.0f);
		m_target_view.rot = normalize(rot * m_target_view.rot);
	}

	void tick(float time_diff) {
		m_view_config = lerp(m_view_config, m_target_view, 0.1f);
		m_anim_pos += time_diff;
	}

	void draw(Renderer &out, Renderer2D &out2d) const {
		out.setProjectionMatrix(perspective(
			degToRad(60.0f), float(m_viewport.width()) / m_viewport.height(), 1.0f, 10000.0f));
		out.setViewMatrix(translation(0, 0, -5.0f));

		const auto &model = m_models[m_current_model];

		auto pose = model.model->animatePose(m_current_anim, m_anim_pos);
		auto initial_bbox = model.model->boundingBox(model.model->animatePose(-1, 0.0f));
		auto bbox = model.model->boundingBox(pose);

		float scale =
			4.0f / max(initial_bbox.width(), max(initial_bbox.height(), initial_bbox.depth()));
		auto matrix = scaling(m_view_config.zoom * scale) * Matrix4(m_view_config.rot) *
					  translation(-initial_bbox.center());
		auto material = model.tex ? Material(model.tex) : Material(m_flat_program, {});
		model.model->draw(out, pose, material, matrix);
		out.addWireBox(initial_bbox, {Color::red}, matrix);
		out.addWireBox(bbox, {Color::green}, matrix);
		if(m_show_nodes)
			model.model->drawNodes(out, pose, Color::green, Color::yellow, matrix);

		TextFormatter fmt;
		fmt("Model: %s (%d / %d)\n", model.model_name.c_str(), m_current_model + 1,
			(int)m_models.size());
		fmt("Texture: %s\n", model.tex ? model.tex_name.c_str() : "none");
		string anim_name = m_current_anim == -1 ? "none" : model.model->anim(m_current_anim).name();
		fmt("Animation: %s (%d / %d)\n", anim_name.c_str(), m_current_anim + 1,
			(int)model.model->animCount());
		fmt("Size: %.2f %.2f %.2f\n\n", initial_bbox.width(), initial_bbox.height(),
			initial_bbox.depth());
		fmt("Help:\n");
		fmt("M: change model\n");
		fmt("A: change animation\n");
		fmt("S: display skeleton\n");
		fmt("up/down/left/right: rotate\n");
		fmt("pgup/pgdn: zoom\n");

		FontRenderer font(m_font_data.first, m_font_data.second, out2d);
		FontStyle style{Color::white, Color::black};
		auto extents = font.font().evalExtents(fmt.text());
		out2d.addFilledRect(FRect(float2(extents.size()) + float2(10, 10)), {Color(0, 0, 0, 80)});
		font.draw(FRect(5, 5, 300, 100), style, fmt.text());
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

	Renderer renderer_3d;
	Renderer2D renderer_2d(s_viewer->viewport());
	s_viewer->draw(renderer_3d, renderer_2d);

	renderer_3d.render();
	renderer_2d.render();

	profilerNextFrame();

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
	} else { files = {make_pair(model_argument, tex_argument)}; }

	auto &gfx_device = GfxDevice::instance();
	gfx_device.createWindow("libfwk::model_viewer", resolution, false);

	double init_time = getTime();
	Viewer viewer(IRect(resolution), files);
	s_viewer = &viewer;

	gfx_device.runMainLoop(main_loop);

	return 0;
}

int main(int argc, char **argv) {
	try {
		return safe_main(argc, argv);
	} catch(const Exception &ex) {
		printf("%s\n\nBacktrace:\n%s\n", ex.what(), cppFilterBacktrace(ex.backtrace()).c_str());
		return 1;
	}
}
