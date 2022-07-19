// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/format.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/font_factory.h>
#include <fwk/gfx/font_finder.h>
#include <fwk/gfx/image.h>
#include <fwk/gfx/renderer2d.h>
#include <fwk/io/file_system.h>
#include <fwk/sys/expected.h>
#include <fwk/sys/input.h>
#include <fwk/vulkan/vulkan_buffer.h>
#include <fwk/vulkan/vulkan_command_queue.h>
#include <fwk/vulkan/vulkan_device.h>
#include <fwk/vulkan/vulkan_image.h>
#include <fwk/vulkan/vulkan_instance.h>
#include <fwk/vulkan/vulkan_pipeline.h>
#include <fwk/vulkan/vulkan_shader.h>
#include <fwk/vulkan/vulkan_swap_chain.h>
#include <fwk/vulkan/vulkan_window.h>

// TODO: what should we do when error happens in begin/end frame?
// TODO: proprly handle multiple queues
// TODO: proprly differentiate between graphics, present & compute queues

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

const char *compute_shader = R"(
#version 450

// TODO: shader constant
#define LSIZE	1024

layout(local_size_x = 1024) in;

layout(binding = 0) buffer buf0_ { uint num_input_elements; uint input_data[]; };
layout(binding = 1) buffer buf1_ { uint num_output_elements; uint output_data[]; };

// TODO push buffer for num_elements

void main() {
	uint num_elements = num_input_elements;
	if(gl_LocalInvocationIndex == 0)
		num_output_elements = num_elements;
	for(uint i = gl_LocalInvocationIndex; i < num_elements; i += LSIZE)
		output_data[i] = input_data[i] + 1;
}
)";

string fontPath() {
	if(platform == Platform::html)
		return "data/LiberationSans-Regular.ttf";
	return findDefaultSystemFont().get().file_path;
}

struct VulkanContext {
	VDeviceRef device;
	VWindowRef window;
	Dynamic<Renderer2D::VulkanPipelines> renderer2d_pipes;
	Maybe<FontCore> font_core;
	PVImage font_image;
	PVImageView font_image_view;
	PVPipeline compute_pipe;
	int compute_buffer_idx = 0;
	PVBuffer compute_buffers[2];
	Maybe<VDownloadId> download_id;
};

Ex<void> drawFrame(VulkanContext &ctx, CSpan<float2> positions, ZStr message) {
	auto &cmds = ctx.device->cmdQueue();
	auto swap_chain = ctx.device->swapChain();
	auto sc_extent = swap_chain->extent();

	// Not drawing if swap chain is not available
	if(swap_chain->status() != VSwapChainStatus::image_acquired)
		return {};

	// TODO: viewport? remove orient ?
	Renderer2D renderer(IRect(sc_extent), Orient2D::y_up);
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
	auto text = format("Window size: %\nVulkan device: %\n", ctx.window->extent(), device_name);
	text += message;
	Font font(*ctx.font_core, ctx.font_image_view);
	font.draw(renderer, FRect({5, 5}, {200, 20}), {ColorId::white}, text);

	auto render_pass =
		ctx.device->getRenderPass({{swap_chain->format(), 1, VColorSyncStd::clear_present}});
	auto dc = EX_PASS(renderer.genDrawCall(ctx.device, *ctx.renderer2d_pipes));
	auto fb = ctx.device->getFramebuffer({swap_chain->acquiredImage()});

	cmds.beginRenderPass(fb, render_pass, none, {{VkClearColorValue{0.0, 0.2, 0.0, 1.0}}});
	cmds.setViewport(IRect(sc_extent));
	cmds.setScissor(none);

	EXPECT(renderer.render(dc, ctx.device, *ctx.renderer2d_pipes));
	cmds.endRenderPass();

	return {};
}

Ex<void> computeStuff(VulkanContext &ctx) {
	auto &cmds = ctx.device->cmdQueue();
	cmds.bind(ctx.compute_pipe->layout(), VBindPoint::compute);
	auto ds = cmds.bindDS(0);
	auto &target_buffer = ctx.compute_buffers[ctx.compute_buffer_idx ^ 1];
	ds(0, ctx.compute_buffers[ctx.compute_buffer_idx]);
	ds(1, target_buffer);
	ctx.compute_buffer_idx ^= 1;

	cmds.bind(ctx.compute_pipe);
	cmds.dispatchCompute({1, 1, 1});
	if(!ctx.download_id)
		ctx.download_id = EX_PASS(cmds.download(target_buffer));
	return {};
}

// Old test_window perf:
// 2260 on AMD Vega   -> 2500 -> 2550 -> 2570 -> 2606
//  880 on RTX 3050   -> 1250 -> 1270 -> 1380 -> 1355
void updateTitleFPS(VulkanWindow &window, ZStr title_prefix) {
	static Maybe<double> old_fps;
	auto fps = window.fps();
	if(fps == old_fps)
		return;
	old_fps = fps;
	string text = title_prefix;
	if(fps)
		text += stdFormat(*fps > 100 ? " [FPS: %.0f]" : " [FPS: %.2f]", *fps);
	window.setTitle(text);
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

	static string last_message;

	if(ctx.download_id) {
		auto &rgraph = ctx.device->cmdQueue();
		if(auto data = rgraph.retrieve(*ctx.download_id)) {
			auto uints = cspan(data).reinterpret<u32>();
			uint count = uints[0];
			bool is_valid = count == uints.size() - 1 && allOf(uints.subSpan(2), uints[1]);
			last_message = format("Compute result: %%\n", uints[1], is_valid ? "" : " (invalid)");
			ctx.download_id = none;
		}
	}

	if(ctx.device->beginFrame().get()) {
		drawFrame(ctx, positions, last_message).check();
		computeStuff(ctx).check();
		ctx.device->finishFrame().check();
	}

	updateTitleFPS(window, "fwk::test_window");

	return true;
}

Ex<PVImage> makeTexture(VulkanContext &ctx, const Image &image) {
	auto vimage = EX_PASS(VulkanImage::create(ctx.device, {VK_FORMAT_R8G8B8A8_SRGB, image.size()},
											  VMemoryUsage::device));
	EXPECT(ctx.device->cmdQueue().upload(vimage, image));
	return vimage;
}

Ex<int> exMain() {
	VulkanInstanceSetup setup;
	setup.debug_levels = VDebugLevel::warning | VDebugLevel::error;
	setup.debug_types = all<VDebugType>;
	auto instance = EX_PASS(VulkanInstance::create(setup));

	auto flags = VWindowFlag::resizable | VWindowFlag::centered | VWindowFlag::allow_hidpi;

	auto window =
		EX_PASS(VulkanWindow::create(instance, "fwk::test_window", IRect(0, 0, 1280, 720), flags));

	VDeviceSetup dev_setup;
	auto pref_device = instance->preferredDevice(window->surfaceHandle(), &dev_setup.queues);
	if(!pref_device)
		return ERROR("Couldn't find a suitable Vulkan device");
	auto device = EX_PASS(instance->createDevice(*pref_device, dev_setup));
	auto phys_info = instance->info(device->physId());
	print("Selected Vulkan device: %\n", phys_info.properties.deviceName);

	auto swap_chain = EX_PASS(VulkanSwapChain::create(
		device, window, {.preferred_present_mode = VPresentMode::immediate}));
	device->addSwapChain(swap_chain);

	VulkanContext ctx{device, window};
	auto compute_module =
		EX_PASS(VulkanShaderModule::compile(device, {{VShaderStage::compute, compute_shader}}));
	ctx.compute_pipe = EX_PASS(VulkanPipeline::create(device, {compute_module[0]}));

	int compute_size = 4 * 1024;
	auto compute_usage =
		VBufferUsage::storage_buffer | VBufferUsage::transfer_dst | VBufferUsage::transfer_src;
	for(auto &buffer : ctx.compute_buffers)
		buffer = EX_PASS(VulkanBuffer::create<u32>(device, compute_size + 1, compute_usage));
	vector<u32> compute_data(compute_size + 1, 0);
	compute_data[0] = compute_size;
	EXPECT(ctx.device->cmdQueue().upload(ctx.compute_buffers[0], compute_data));

	int font_size = 16 * window->dpiScale();
	auto font_data = EX_PASS(FontFactory().makeFont(fontPath(), font_size));
	ctx.font_core = move(font_data.core);
	ctx.font_image = EX_PASS(makeTexture(ctx, font_data.image));
	ctx.font_image_view = VulkanImageView::create(ctx.device, ctx.font_image);

	ctx.renderer2d_pipes =
		EX_PASS(Renderer2D::makeVulkanPipelines(ctx.device, swap_chain->format()));

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