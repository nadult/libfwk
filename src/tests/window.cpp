// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/format.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/font_factory.h>
#include <fwk/gfx/font_finder.h>
#include <fwk/gfx/gl_texture.h>
#include <fwk/gfx/image.h>
#include <fwk/gfx/renderer2d.h>
#include <fwk/io/file_system.h>
#include <fwk/slab_allocator.h>
#include <fwk/sys/expected.h>
#include <fwk/sys/input.h>
#include <fwk/vulkan/vulkan_buffer.h>
#include <fwk/vulkan/vulkan_device.h>
#include <fwk/vulkan/vulkan_framebuffer.h>
#include <fwk/vulkan/vulkan_image.h>
#include <fwk/vulkan/vulkan_instance.h>
#include <fwk/vulkan/vulkan_memory_manager.h>
#include <fwk/vulkan/vulkan_pipeline.h>
#include <fwk/vulkan/vulkan_render_graph.h>
#include <fwk/vulkan/vulkan_shader.h>
#include <fwk/vulkan/vulkan_swap_chain.h>
#include <fwk/vulkan/vulkan_window.h>

#include <vulkan/vulkan.h>

// TODO: minimize errors, if possible; Unified handling of OOM errors? (both host & device?)

// Czy zostawiæ OGL czy nie ?
// Nie bêdê maintainowaæ dwóch wersji...
// Jak ju¿ mam sensown¹ obs³ugê vulkana (innej nie chcê) to ogl jest mi po nic...
// Na macu i tak bym u¿ywa³ moltenVk
// wiêc mo¿e ju¿ teraz zacznijmy to przerabiaæ stopniowo wywalaj¹c stary oglowy kod ?

// Co jeszcze musze zrobiæ?
// - sensowne tworzenie obiektów pipeline-a i render-passy
//   - automatyczne ustawianie layoutów ?
//
// - na razie mo¿emy olaæ subpassy
//
// - jakieœ cachew-owanie renderpassów i pipelineów?
//   jak ?
//
// - jeœli mam renderer2D, to jak to konkretnie ma dzia³aæ ?
//   renderujê na ekran, a co jeœli mam podpiêty depth buffer ?
//   po prostu ignorujê depth buffer?
//
// - render2D wogóle nie powinien sam generowaæ komend, jedynie draw calle i ew. móg³by definiowac pipeline ?
//   albo elementy pipeline-a .
//
//   u¿ytownik by sobie sk³ada³ te elementy w ca³y render pass ?
//   Czy chcê render passy cache-owaæ ? moze nie koniecznie ?
//
// ³atwe konstruowanie opisów render passów i pipeline-ów
//
// z opisów muszê byæ w stanie szybko znaleŸæ odpowiedni obiekt pipeline-a, ¿eby go u¿yæ w komendach
//

// TODO: what should we do when error happens in begin/end frame?
// TODO: proprly handle multiple queues
// TODO: proprly differentiate between graphics, present & compute queues

// TODO: handling VkResults
// TODO: making sure that handles are correct ?
// TODO: more type safety
// TODO: VHandle<> class ? handles are not initialized to null by default ...
// TODO: test_vulkan na nvidii jest ca³y czas wolniejszy ni¿ test_window (1300 vs 2220 FPS)
// TODO: mo¿na jeszcze pewnie zrobiæ du¿o ró¿nego rodzaju optymalizacji, ale najpierw przyda³yby
//       siê jakieœ konkretne programy testowe
// TODO: add support for dedicated allocations in MemoryManager
// TODO: robust mapping / unmapping in MemoryManager
// TODO: garbage collection in memory manager (especially chunks), but first we should test it more thoroughly

using namespace fwk;

const char *vertex_shader = R"(
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoord;

layout(binding = 0) uniform UniformBufferObject {
	float saturation;
} ubo;

void main() {
	gl_Position = vec4(inPosition, 0.0, 1.0);
	fragColor = inColor * (1.0 - ubo.saturation) + vec3(1.0) * ubo.saturation;
	texCoord = inTexCoord;
}
)";

const char *fragment_shader = R"(
#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoord;

layout(binding = 1) uniform sampler2D texSampler;

void main() {
	outColor = vec4(fragColor, 1.0) * texture(texSampler, texCoord);
}
)";

string fontPath() {
	if(platform == Platform::html)
		return "data/LiberationSans-Regular.ttf";
	return findDefaultSystemFont().get().file_path;
}

struct MyVertex {
	float2 pos;
	float3 color;
	float2 texCoord;

	static void addStreamDesc(vector<VVertexBinding> &bindings, vector<VVertexAttrib> &attribs,
							  int index, int first_location) {
		bindings.emplace_back(vertexBinding<MyVertex>(index));
		attribs.emplace_back(vertexAttrib<float2>(first_location, index, 0));
		attribs.emplace_back(vertexAttrib<float3>(first_location + 1, index, sizeof(pos)));
		attribs.emplace_back(
			vertexAttrib<float2>(first_location + 2, index, sizeof(pos) + sizeof(color)));
	}
};

struct VulkanContext {
	VDeviceRef device;
	VWindowRef window;
	PVPipeline pipeline;
	Dynamic<Renderer2D::VulkanPipelines> renderer2d_pipes;
	Maybe<FontCore> font_core;
	PVImage font_image;
	PVImageView font_image_view;
	PVSampler sampler;
};

Ex<void> createPipeline(VulkanContext &ctx) {
	Pair<VShaderStage, ZStr> source_codes[] = {{VShaderStage::vertex, vertex_shader},
											   {VShaderStage::fragment, fragment_shader}};
	auto shader_modules = EX_PASS(VulkanShaderModule::compile(ctx.device, source_codes));

	auto &render_graph = ctx.device->renderGraph();
	auto swap_chain = render_graph.swapChain();
	auto sc_image = swap_chain->imageViews().front()->image();
	auto extent = sc_image->extent();
	VPipelineSetup setup;
	setup.shader_modules = shader_modules;
	// TODO: maybe pass ColorAttachment directly?
	setup.render_pass = ctx.device->getRenderPass({{sc_image->format(), 1}});
	setup.viewport = {IRect(extent)};
	setup.raster = {VPrimitiveTopology::triangle_list, VPolygonMode::fill, VCull::back,
					VFrontFace::cw};
	MyVertex::addStreamDesc(setup.vertex_bindings, setup.vertex_attribs, 0, 0);

	ctx.pipeline = EX_PASS(VulkanPipeline::create(ctx.device, setup));
	return {};
}

Ex<PVBuffer> createVertexBuffer(VulkanContext &ctx, CSpan<MyVertex> vertices) {
	auto usage = VBufferUsage::vertex_buffer | VBufferUsage::transfer_dst;
	return VulkanBuffer::create<MyVertex>(ctx.device, vertices.size(), usage, VMemoryUsage::frame);
}

struct UBOData {
	float saturation = 1.0;
	float temp[15];
};

Ex<PVBuffer> createUniformBuffer(VulkanContext &ctx) {
	auto usage = VBufferUsage::uniform_buffer | VBufferUsage::transfer_dst;
	return VulkanBuffer::create<UBOData>(ctx.device, 1, usage, VMemoryUsage::frame);
}

Ex<void> drawFrame(VulkanContext &ctx, CSpan<float2> positions) {
	ctx.device->beginFrame();
	auto &render_graph = ctx.device->renderGraph();

	//UBOData ubo;
	//ubo.saturation = 0.25 + sin(getTime()) * 0.25;
	//auto ubuffer = EX_PASS(createUniformBuffer(ctx));
	//EXPECT(render_graph << CmdUpload(cspan(&ubo, 1), ubuffer));

	auto sc_format = render_graph.swapChainFormat();
	auto render_pass = ctx.device->getRenderPass({{sc_format, 1, VColorSyncStd::clear_present}});

	// TODO: viewport? remove orient ?
	Renderer2D renderer(ctx.renderer2d_pipes->viewport, Orient2D::y_up);
	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect({-50, -50}, {50, 50}) + positions[n];
		FColor fill_color(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f);
		IColor border_color = ColorId::black;

		renderer.addFilledRect(rect, fill_color);
		renderer.addRect(rect, border_color);
	}

	auto device_name = VulkanInstance::ref()->info(ctx.device->physId()).properties.deviceName;
	// TODO: immediate drawing mode sux, we should replace it with something better?
	// On the other hand, imgui is fine...
	auto text =
		format("Hello world!\nWindow size: %\nVulkan device: %", ctx.window->extent(), device_name);
	Font font(*ctx.font_core, ctx.font_image_view);
	font.draw(renderer, FRect({5, 5}, {200, 20}), {ColorId::white}, text);

	auto dc = EX_PASS(renderer.genDrawCall(ctx.device, *ctx.renderer2d_pipes));
	auto fb = render_graph.defaultFramebuffer();

	render_graph << CmdBeginRenderPass{
		fb, render_pass, none, {{VkClearColorValue{0.0, 0.2, 0.0, 1.0}}}};
	EXPECT(renderer.render(dc, ctx.device, *ctx.renderer2d_pipes));
	render_graph << CmdEndRenderPass{};
	// TODO: final layout has to be present, middle layout shoud be different FFS! How to automate this ?!?
	// Only queue uploads ?

	ctx.device->finishFrame();

	return {};
}

// Old test_window perf:
// 2260 on AMD Vega   -> 2500 -> 2550 -> 2570
//  880 on RTX 3050   -> 1250 -> 1270 -> 1380
void updateFPS(VulkanWindow &window) {
	static double prev_time = getTime();
	static int num_frames = 0;
	double cur_time = getTime();
	num_frames++;
	if(cur_time - prev_time > 1.0) {
		int fps = double(num_frames) / (cur_time - prev_time);
		window.setTitle(format("fwk::test_window [FPS: %]", fps));
		prev_time = cur_time;
		num_frames = 0;
	}
}

bool mainLoop(VulkanWindow &window, void *ctx_) {
	auto &ctx = *(VulkanContext *)ctx_;
	static vector<float2> positions(15, float2(window.extent() / 2));

	for(auto &event : window.inputEvents()) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEventType::quit)
			return false;

		if(event.isMouseOverEvent() && (event.mouseMove() != int2(0, 0)))
			positions.emplace_back(float2(event.mousePos()));
	}

	while(positions.size() > 15)
		positions.erase(positions.begin());

	auto viewport = IRect(ctx.window->rect().size());
	if(!ctx.renderer2d_pipes || viewport != ctx.renderer2d_pipes->viewport) {
		auto sc_format = ctx.device->renderGraph().swapChainFormat();
		ctx.renderer2d_pipes =
			Renderer2D::makeVulkanPipelines(ctx.device, sc_format, viewport).get();
	}

	drawFrame(ctx, positions).check();

	updateFPS(window);

	return true;
}

Ex<PVImage> makeTexture(VulkanContext &ctx, const Image &image) {
	auto vimage = EX_PASS(VulkanImage::create(ctx.device, {VK_FORMAT_R8G8B8A8_SRGB, image.size()},
											  VMemoryUsage::device));
	EXPECT(ctx.device->renderGraph() << CmdUploadImage(image, vimage));
	return vimage;
}

Ex<int> exMain() {
	VulkanInstanceSetup setup;
	setup.debug_levels = VDebugLevel::warning | VDebugLevel::error;
	setup.debug_types = all<VDebugType>;
	auto instance = EX_PASS(VulkanInstance::create(setup));

	// TODO: cleanup in flags
	auto flags = VWindowFlag::resizable | VWindowFlag::vsync | VWindowFlag::centered |
				 VWindowFlag::allow_hidpi;
	auto window = EX_PASS(VulkanWindow::create(instance, "fwk::test_window", IRect(0, 0, 1280, 720),
											   VulkanWindowConfig{flags}));

	VulkanDeviceSetup dev_setup;
	auto pref_device = instance->preferredDevice(window->surfaceHandle(), &dev_setup.queues);
	if(!pref_device)
		return ERROR("Couldn't find a suitable Vulkan device");
	auto device = EX_PASS(instance->createDevice(*pref_device, dev_setup));
	auto phys_info = instance->info(device->physId());
	print("Selected Vulkan physical device: %\nDriver version: %\n",
		  phys_info.properties.deviceName, phys_info.properties.driverVersion);

	auto swap_chain = VulkanSwapChain::create(device, window,
											  {.preferred_present_mode = VPresentMode::immediate});
	EXPECT(device->createRenderGraph(swap_chain));

	VulkanContext ctx{device, window};
	EXPECT(createPipeline(ctx));

	int font_size = 16 * window->dpiScale();
	auto font_data = EX_PASS(FontFactory().makeFont(fontPath(), font_size));
	ctx.font_core = move(font_data.core);
	ctx.font_image = EX_PASS(makeTexture(ctx, font_data.image));
	ctx.font_image_view = VulkanImageView::create(ctx.device, ctx.font_image);
	ctx.sampler = ctx.device->createSampler({});

	window->runMainLoop(mainLoop, &ctx);
	return 0;
}

int main(int argc, char **argv) {
	auto result = exMain();

	if(!result) {
		result.error().print();
		return 1;
	}
	return *result;
}