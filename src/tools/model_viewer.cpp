// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <stdio.h>

#include <fwk/gfx/animated_model.h>
#include <fwk/gfx/canvas_2d.h>
#include <fwk/gfx/canvas_3d.h>
#include <fwk/gfx/drawing.h>
#include <fwk/gfx/dynamic_mesh.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/image.h>
#include <fwk/gfx/mesh.h>
#include <fwk/gfx/model.h>
#include <fwk/gfx/shader_compiler.h>
#include <fwk/io/file_system.h>
#include <fwk/math/axis_angle.h>
#include <fwk/math/rotation.h>
#include <fwk/sys/input.h>
#include <fwk/vulkan/vulkan_command_queue.h>
#include <fwk/vulkan/vulkan_device.h>
#include <fwk/vulkan/vulkan_image.h>
#include <fwk/vulkan/vulkan_instance.h>
#include <fwk/vulkan/vulkan_swap_chain.h>
#include <fwk/vulkan/vulkan_window.h>

#ifndef FWK_IMGUI_DISABLED
#include <fwk/gfx/font_finder.h>
#include <fwk/gui/gui.h>
#include <fwk/gui/imgui.h>
#include <fwk/gui/widgets.h>
#endif

using namespace fwk;

static string dataPath(string file_name) {
#ifdef FWK_PLATFORM_MSVC
	auto main_path = FilePath::current().get();
#else
	auto main_path = FilePath(executablePath()).parent().parent();
#endif
	return main_path / "data" / file_name;
}

static void loadShaderDefs(ShaderCompiler &compiler) {
	vector<Pair<string>> vsh_macros = {{"VERTEX_SHADER", "1"}};
	vector<Pair<string>> fsh_macros = {{"FRAGMENT_SHADER", "1"}};

	compiler.add({"ray_trace", VShaderStage::compute, "ray_trace.shader"});
	compiler.add({"display_vert", VShaderStage::vertex, "display.shader", vsh_macros});
	compiler.add({"display_frag", VShaderStage::vertex, "display.shader", fsh_macros});
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
	ViewModel(Model model, Maybe<PVImageView> tex, string mname, string tname)
		: m_model(std::move(model)), m_model_name(mname), m_tex_name(tname), m_num_segments(0) {
		HashMap<string, SimpleMaterial> map;
		if(tex)
			m_default_material = {*tex};

		for(const auto &def : m_model.materialDefs()) {
			map[def.name] = tex ? SimpleMaterial(*tex, IColor(def.diffuse)) :
								  SimpleMaterial(IColor(def.diffuse));
		}
		m_materials = std::move(map);
	}

	Pose animatePose(int anim_id, float anim_pos) const {
		return m_model.animatePose(anim_id, anim_pos);
	}

	auto animate(const Pose &pose) const { return AnimatedModel(m_model, pose); }
	FBox boundingBox(const Pose &pose) const { return animate(pose).boundingBox(); }
	FBox boundingBox() const { return boundingBox(m_model.defaultPose()); }

	float scale() const { return 4.0f / max(boundingBox().size().values()); }

	void drawModel(Canvas3D &out, const Pose &pose, const Matrix4 &matrix) {
		//out.add(animate(pose).genDrawCalls(m_materials, matrix));
	}

	//void drawNodes(TriangleBuffer &tris, LineBuffer &lines, const Pose &pose) {
	//	m_model.drawNodes(tris, lines, pose, ColorId::green, ColorId::yellow, 0.1f / scale());
	//}

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

	Model m_model;
	HashMap<string, SimpleMaterial> m_materials;
	SimpleMaterial m_default_material;
	string m_model_name;
	string m_tex_name;
	int m_num_segments;
};

HashMap<string, PVImageView> loadImages(VulkanDevice &device, CSpan<Pair<string>> file_names) {
	HashMap<string, PVImageView> out;

	for(auto pair : file_names) {
		auto tex_name = pair.second;
		if(tex_name.empty() || !access(tex_name))
			continue;

		auto image = Image::load(tex_name);
		if(!image) {
			image.error().print();
			image = Image(int2(4, 4), ColorId::magneta);
		}

		auto tex_view = VulkanImage::createAndUploadWithView(device, {*image});
		if(!tex_view) {
			tex_view.error().print();
			continue;
		}
		out.emplace(tex_name, *tex_view);
	}
	return out;
}

PVRenderPass guiRenderPass(VDeviceRef device) {
	auto sc_format = device->swapChain()->format();
	return device->getRenderPass({{sc_format, VSimpleSync::draw}});
}

class Viewer {
  public:
	void updateViewport() { m_viewport = IRect(m_window->size()); }

	Viewer(VWindowRef window, VDeviceRef device, const vector<Pair<string>> &file_names)
		: m_window(window), m_device(device), m_current_model(0), m_current_anim(-1),
		  m_anim_pos(0.0), m_show_nodes(false) {
		updateViewport();

		auto images = loadImages(*device, file_names);

		for(auto file_name : file_names) {
			auto image = images.maybeFind(file_name.second);

			double time = getTime();
			auto model = Model::load(file_name.first);
			time = (getTime() - time) * 1000.0;

			if(model) {
				printf("Loaded %s: %.2f ms\n", file_name.first.c_str(), time);
				m_models.emplace_back(std::move(model.get()), image, file_name.first,
									  file_name.second);
			} else {
				print("Error while loading '%':\n%", file_name.first, model.error());
			}
		}

#ifndef FWK_IMGUI_DISABLED
		auto font_info = findDefaultSystemFont().get();
		GuiConfig gui_config;
		gui_config.style_mode = GuiStyleMode::normal;
		gui_config.font_path = font_info.file_path;
		m_gui.emplace(device, window, guiRenderPass(device), gui_config);
#else
		int font_scale = 14;
		m_font.emplace(Font::makeDefault(device, window, font_scale).get());
#endif
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

#ifdef FWK_IMGUI_DISABLED
	void helpBox(Canvas2D &out, ViewModel &model) const {
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
		out.addFilledRect(FRect(float2(extents.size()) + float2(10, 10)), {IColor(0, 0, 0, 80)});
		m_font->draw(out, FRect({5, 5}, {300, 100}), style, fmt.text());
	}
#endif

	void doMenu() {
#ifndef FWK_IMGUI_DISABLED
		static bool set_pos = true;
		if(set_pos) {
			ImGui::SetNextWindowPos(int2());
			ImGui::SetNextWindowSize({350, 300});
			set_pos = false;
		}
		ImGui::Begin("Control", nullptr,
					 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		m_gui->text("Model:");
		ImGui::SameLine();
		if(ImGui::BeginCombo("##model", m_models[m_current_model].m_model_name.c_str())) {
			for(int n : intRange(m_models))
				if(ImGui::Selectable(m_models[n].m_model_name.c_str(), n == m_current_model))
					m_current_model = n;
			ImGui::EndCombo();
		}

		auto &model = m_models[m_current_model];
		auto &anims = model.m_model.anims();
		m_gui->text("Animation:");
		ImGui::SameLine();
		const char *cur_anim =
			!anims.inRange(m_current_anim) ? "empty" : anims[m_current_anim].name().c_str();
		if(ImGui::BeginCombo("##anim", cur_anim)) {
			if(ImGui::Selectable("empty", m_current_anim == -1))
				m_current_anim = -1;
			for(int n : intRange(anims))
				if(ImGui::Selectable(anims[n].name().c_str(), m_current_anim == n))
					m_current_anim = n;
			ImGui::EndCombo();
		}

		ImGui::Separator();
		m_gui->text(helpText());
		ImGui::End();
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

	bool hasModels() const { return !!m_models; }

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
		m_device->beginFrame().check();
		auto swap_chain = m_device->swapChain();
		if(swap_chain->status() == VSwapChainStatus::image_acquired) {
			IColor nice_background(200, 200, 255);
			//clearColor(nice_background);
			//clearDepth(1.0f);

			Matrix4 proj = perspective(
				degToRad(60.0f), float(m_viewport.width()) / m_viewport.height(), 1.0f, 10000.0f);
			Matrix4 view = translation(0, 0, -5.0f);
			Canvas3D renderer_3d(m_viewport, proj, view);
			auto &model = m_models[m_current_model];

			auto anim_id = model.m_model.anims().inRange(m_current_anim) ? m_current_anim : -1;
			auto pose = model.animatePose(anim_id, m_anim_pos);
			auto matrix = scaling(m_view_config.zoom * model.scale()) * Matrix4(m_view_config.rot) *
						  translation(-model.boundingBox().center());

			renderer_3d.setViewMatrix(matrix);

			/*model.drawModel(renderer_3d, pose, matrix);
			if(m_show_nodes)
				model.drawNodes(tris, lines, pose);
			renderer_3d.add
			lines(model.boundingBox(pose), ColorId::green);

			renderer_3d.add(tris.drawCalls());
			renderer_3d.add(lines.drawCalls());

			renderer_3d.render();*/

			auto &cmds = m_device->cmdQueue();
			auto fb = m_device->getFramebuffer({m_device->swapChain()->acquiredImage()});
			cmds.beginRenderPass(fb, guiRenderPass(m_device), none);

#ifdef FWK_IMGUI_DISABLED
			Renderer2D renderer_2d(m_viewport, Orient2D::y_down);
			helpBox(renderer_2d, model);
			renderer_2d.render();
#else
			m_gui->drawFrame(*m_window, cmds.bufferHandle());
#endif

			cmds.endRenderPass();
		}
		m_device->finishFrame().check();
	}

	const IRect &viewport() const { return m_viewport; }

	bool mainLoop() {
		vector<InputEvent> events;
#ifdef FWK_IMGUI_DISABLED
		events = device.inputEvents();
#else
		m_gui->beginFrame(*m_window);
		doMenu();
		events = m_gui->finishFrame(*m_window);
#endif

		float time_diff = 1.0f / 60.0f;
		if(!handleInput(events, time_diff))
			return false;
		tick(time_diff);
		updateViewport();
		draw();

#ifndef FWK_IMGUI_DISABLED
		m_gui->endFrame();
#endif

		return true;
	}

	static bool mainLoop(VulkanWindow &window, void *this_ptr) {
		return ((Viewer *)this_ptr)->mainLoop();
	}

  private:
	VWindowRef m_window;
	VDeviceRef m_device;

	vector<ViewModel> m_models;
#ifndef FWK_IMGUI_DISABLED
	Dynamic<Gui> m_gui;
#else
	Dynamic<Font> m_font;
#endif

	IRect m_viewport;
	int m_current_model, m_current_anim;
	double m_anim_pos;
	bool m_show_nodes;
	ViewConfig m_view_config;
	ViewConfig m_target_view;
};

Ex<> exMain(int argc, char **argv) {
	double time = getTime();

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
		prefix = FilePath(prefix);

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

	auto displays = VulkanWindow::displays();
	if(!displays)
		FWK_FATAL("No display available");
	int2 resolution = displays[0].rect.size() * 2 / 3;

	// TODO: multisampling
	VInstanceSetup setup;
#ifndef NDEBUG
	setup.debug_levels = VDebugLevel::warning | VDebugLevel::error;
	setup.debug_types = all<VDebugType>;
#endif
	auto window_flags = VWindowFlag::resizable | VWindowFlag::centered | VWindowFlag::allow_hidpi |
						VWindowFlag::sleep_when_minimized;
	VSwapChainSetup swap_chain_setup;
	swap_chain_setup.preferred_formats = {VK_FORMAT_B8G8R8A8_UNORM};
	swap_chain_setup.preferred_present_mode = VPresentMode::immediate;
	swap_chain_setup.usage =
		VImageUsage::color_att | VImageUsage::storage | VImageUsage::transfer_dst;
	swap_chain_setup.initial_layout = VImageLayout::general;

	// TODO: create instance on a thread, in the meantime load resources?
	auto instance = EX_PASS(VulkanInstance::create(setup));

	auto window = EX_PASS(
		VulkanWindow::create(instance, "libfwk::model_viewer", IRect(resolution), window_flags));

	VDeviceSetup dev_setup;
	auto pref_device = instance->preferredDevice(window->surfaceHandle(), &dev_setup.queues);
	if(!pref_device)
		return FWK_ERROR("Couldn't find a suitable Vulkan device");
	dev_setup.extensions = {"VK_EXT_shader_subgroup_vote", "VK_EXT_shader_subgroup_ballot"};
	auto device = EX_PASS(instance->createDevice(*pref_device, dev_setup));
	auto phys_info = instance->info(device->physId());
	print("Selected Vulkan physical device: %\nDriver version: %\n",
		  phys_info.properties.deviceName, phys_info.properties.driverVersion);
	device->addSwapChain(EX_PASS(VulkanSwapChain::create(*device, window, swap_chain_setup)));

	double init_time = getTime();
	Viewer viewer(window, device, files);
	if(!viewer.hasModels()) {
		print("No models\n");
		return {};
	}

	window->runMainLoop(Viewer::mainLoop, &viewer);
	return {};
}

int main(int argc, char **argv) {
	auto result = exMain(argc, argv);

	if(!result) {
		result.error().print();
		return 1;
	}
	return 0;
}
