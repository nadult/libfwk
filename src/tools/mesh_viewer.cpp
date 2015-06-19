/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk.h"

using namespace fwk;

namespace {
ResourceManager<SkinnedMesh, XMLLoader<SkinnedMesh>> s_meshes("", "", "");
ResourceManager<DTexture> s_textures("", "");
}

struct ViewConfig {
	ViewConfig(float zoom = 1.0f, float x_rot = 0.0f, float y_rot = 0.0f)
		: zoom(zoom), x_rot(x_rot), y_rot(y_rot) {}

	float zoom;
	float x_rot, y_rot;
};

ViewConfig lerp(const ViewConfig &a, const ViewConfig &b, float t) {
	return ViewConfig(lerp(a.zoom, b.zoom, t), lerp(a.x_rot, b.x_rot, t),
					  lerp(a.y_rot, b.y_rot, t));
}

class Viewer {
  public:
	struct Mesh {
		PSkinnedMesh mesh;
		PTexture tex;
		string mesh_name;
		string tex_name;
	};

	Viewer(const IRect &viewport, const vector<pair<string, string>> &file_names)
		: m_viewport(viewport), m_current_mesh(0), m_current_anim(-1), m_anim_pos(0.0) {
		for(auto file_name : file_names) {

			PTexture tex;
			if(!file_name.second.empty()) {
				try {
					tex = s_textures[file_name.second];
				} catch(...) {}
			}
			m_meshes.emplace_back(
				Mesh{s_meshes[file_name.first], tex, file_name.first, file_name.second});
		}

		FontFactory factory;
		m_font_data = factory.makeFont("data/LiberationSans-Regular.ttf", 14, false);

		if(m_meshes.empty())
			THROW("No meshes loaded\n");
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
				m_current_mesh = (m_current_mesh + 1) % m_meshes.size();
				m_current_anim = -1;
				m_anim_pos = 0.0;
			}
			if(event.keyDown('a')) {
				m_current_anim++;
				if(m_current_anim == m_meshes[m_current_mesh].mesh->animCount())
					m_current_anim = -1;
				m_anim_pos = 0.0f;
			}
		}

		m_target_view.zoom = clamp(m_target_view.zoom * (1.0f + scale), 0.2f, 4.0f);
		m_target_view.x_rot += x_rot;
		m_target_view.y_rot += y_rot;
	}

	void tick(float time_diff) {
		m_view_config = lerp(m_view_config, m_target_view, 0.1f);
		m_anim_pos += time_diff;
	}

	void draw(Renderer &out, Renderer2D &out2d) const {
		out.setProjectionMatrix(perspective(
			degToRad(60.0f), float(m_viewport.width()) / m_viewport.height(), 1.0f, 10000.0f));
		out.setViewMatrix(translation(0, 0, -5.0f));

		const auto &mesh = m_meshes[m_current_mesh];

		auto pose = mesh.mesh->animateSkeleton(m_current_anim, m_anim_pos);
		auto bbox = mesh.mesh->boundingBox(mesh.mesh->animateSkeleton(-1, 0.0f));
		float scale = 10.0f / (bbox.width() + bbox.height() + bbox.depth());
		auto matrix = scaling(m_view_config.zoom * scale) *
					  Matrix4(Quat(AxisAngle({0, 1, 0}, m_view_config.x_rot)) *
							  Quat(AxisAngle({1, 0, 0}, m_view_config.y_rot))) *
					  translation(-bbox.center());
		mesh.mesh->draw(out, pose, {mesh.tex}, matrix);
		out.addWireBox(((fwk::Mesh *)(mesh.mesh.get()))->boundingBox(), {Color::red}, matrix);
		out.addWireBox(bbox, {Color::green}, matrix);

		TextFormatter fmt;
		fmt("Mesh: %s (%d / %d)\n", mesh.mesh_name.c_str(), m_current_mesh + 1,
			(int)m_meshes.size());
		fmt("Texture: %s\n", mesh.tex ? mesh.tex_name.c_str() : "none");
		string anim_name = m_current_anim == -1 ? "none" : mesh.mesh->anim(m_current_anim).name();
		fmt("Animation: %s (%d / %d)\n\n", anim_name.c_str(), m_current_anim + 1,
			(int)mesh.mesh->animCount());
		fmt("Help:\n");
		fmt("M: change mesh\n");
		fmt("A: change animation\n");
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
	vector<Mesh> m_meshes;
	pair<PFont, PTexture> m_font_data;

	IRect m_viewport;
	int m_current_mesh, m_current_anim;
	double m_anim_pos;
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
		printf("Usage:\n%s mesh_name.mesh\n%s data/*.mesh\n", argv[0], argv[0]);
		return 0;
	}

	string mesh_argument = argv[1], tex_argument = argc > 2 ? argv[2] : "";
	vector<pair<string, string>> files;

	if(mesh_argument.find('*') != string::npos) {
		auto star_pos = mesh_argument.find('*');
		string prefix = mesh_argument.substr(0, star_pos);
		string suffix = mesh_argument.substr(star_pos + 1);

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
	} else { files = {make_pair(mesh_argument, tex_argument)}; }

	auto &gfx_device = GfxDevice::instance();
	gfx_device.createWindow("libfwk::mesh_viewer", resolution, false);

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
