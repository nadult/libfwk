// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/format.h>
#include <fwk/gfx/canvas_2d.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/image.h>
#include <fwk/gfx/shader_compiler.h>
#include <fwk/io/file_system.h>
#include <fwk/sys/expected.h>
#include <fwk/sys/input.h>
#include <fwk/vulkan/vulkan_buffer_span.h>
#include <fwk/vulkan/vulkan_command_queue.h>
#include <fwk/vulkan/vulkan_device.h>
#include <fwk/vulkan/vulkan_image.h>
#include <fwk/vulkan/vulkan_instance.h>
#include <fwk/vulkan/vulkan_pipeline.h>
#include <fwk/vulkan/vulkan_shader.h>
#include <fwk/vulkan/vulkan_swap_chain.h>
#include <fwk/vulkan/vulkan_window.h>

#include <fwk/gui/gui.h>
#include <fwk/perf/analyzer.h>
#include <fwk/perf/manager.h>
#include <fwk/perf/thread_context.h>

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

struct VulkanContext {
	VDeviceRef device;
	VWindowRef window;
	ShaderCompiler &compiler;
	Maybe<Font> font;
	PVPipeline compute_pipe;
	int compute_buffer_idx = 0;
	VBufferSpan<u32> compute_buffers[2];
	Maybe<VDownloadId> download_id;
	Gui *gui;
	perf::Analyzer *perf_analyzer = nullptr;
};

Ex<void> drawFrame(VulkanContext &ctx, CSpan<float2> positions, ZStr message) {
	auto &cmds = ctx.device->cmdQueue();
	PERF_GPU_SCOPE(cmds);

	auto swap_chain = ctx.device->swapChain();
	auto sc_extent = swap_chain->extent();

	// Not drawing if swap chain is not available
	if(swap_chain->status() != VSwapChainStatus::image_acquired)
		return {};

	// TODO: viewport? remove orient ?
	Canvas2D canvas(IRect(sc_extent), Orient2D::y_up);
	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect({-50, -50}, {50, 50}) + positions[n];
		FColor fill_color(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f);
		IColor border_color = ColorId::black;

		canvas.addFilledRect(rect, fill_color);
		canvas.addRect(rect, border_color);
	}

	auto device_name = ctx.device->physInfo().properties.deviceName;
	auto text = format("Window size: %\nVulkan device: %\n", ctx.window->extent(), device_name);
	text += message;
	ctx.font->draw(canvas, FRect({5, 5}, {200, 20}), {ColorId::white, ColorId::black}, text);

	auto render_pass =
		ctx.device->getRenderPass({{swap_chain->format(), 1, VColorSyncStd::clear_present}});
	auto dc = EX_PASS(canvas.genDrawCall(ctx.compiler, *ctx.device, render_pass));
	auto fb = ctx.device->getFramebuffer({swap_chain->acquiredImage()});

	cmds.beginRenderPass(fb, render_pass, none, {FColor(0.0, 0.2, 0.0)});
	cmds.setViewport(IRect(sc_extent));
	cmds.setScissor(none);

	dc.render(*ctx.device);

	PERF_CHILD_SCOPE("imgui_draw");
	ctx.gui->drawFrame(*ctx.window, cmds.bufferHandle());
	PERF_CLOSE_SCOPE();

	cmds.endRenderPass();

	return {};
}

Ex<void> computeStuff(VulkanContext &ctx) {
	auto &cmds = ctx.device->cmdQueue();
	PERF_GPU_SCOPE(cmds);

	cmds.bind(ctx.compute_pipe);
	auto ds = cmds.bindDS(0);
	auto &target_buffer = ctx.compute_buffers[ctx.compute_buffer_idx ^ 1];
	ds(0, ctx.compute_buffers[ctx.compute_buffer_idx], target_buffer);
	ctx.compute_buffer_idx ^= 1;
	cmds.dispatchCompute({1, 1, 1});
	if(!ctx.download_id)
		ctx.download_id = EX_PASS(cmds.download(target_buffer));
	return {};
}

// Old test_window perf:
// 2260 on AMD Vega   -> 2500 -> 2550 -> 2570 -> 2606 -> 2560 (with compute)
//  880 on RTX 3050   -> 1250 -> 1270 -> 1380 -> 1355 -> 1180 (with compute)
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
	perf::nextFrame();
	perf::Manager::instance()->getNewFrames();

	PERF_SCOPE();

	auto &ctx = *(VulkanContext *)ctx_;
	static vector<float2> positions(15, float2(window.extent() / 2));

	// TODO: begin & finishFrame w gui znacz¹ coœ innego ni¿ w devce; nazwijmy to inaczej
	ctx.gui->beginFrame(window);
	if(ctx.perf_analyzer) {
		bool show = true;
		ctx.perf_analyzer->doMenu(show);
	}
	auto events = ctx.gui->finishFrame(window);

	for(auto &event : events) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEventType::quit)
			return false;

		if(event.isMouseOverEvent() && (event.mouseMove() != int2(0, 0)))
			positions.emplace_back(float2(event.mousePos()));
	}

	while(positions.size() > 15)
		positions.erase(positions.begin());

	static string last_message;
	static int num_invalid = 0;

	if(ctx.download_id) {
		auto &cmds = ctx.device->cmdQueue();
		if(auto uints = cmds.retrieve<u32>(*ctx.download_id)) {
			uint count = uints[0];
			bool is_valid = count == uints.size() - 1 && allOf(subSpan(uints, 2), uints[1]);
			last_message = format("Compute result: %%\n", uints[1], is_valid ? "" : " (invalid)");
			if(!is_valid)
				num_invalid++;
			if(num_invalid > 0)
				last_message += format("Invalid computations: %\n", num_invalid);
			ctx.download_id = none;
		}
	}

	string message = last_message;

	PERF_CHILD_SCOPE("process_frame");
	ctx.device->beginFrame().check();
	drawFrame(ctx, positions, message).check();
	computeStuff(ctx).check();
	ctx.device->finishFrame().check();
	ctx.gui->endFrame();

	PERF_SIBLING_SCOPE("update_title");
	updateTitleFPS(window, "fwk::test_window");

	return true;
}

Ex<int> exMain() {
	VInstanceSetup setup;
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
	auto phys_info = device->physInfo();
	print("Selected Vulkan device: %\n", phys_info.properties.deviceName);

	auto swap_chain = EX_PASS(VulkanSwapChain::create(
		device, window, {.preferred_present_mode = VPresentMode::immediate}));
	device->addSwapChain(swap_chain);

	ShaderCompiler compiler;

	VulkanContext ctx{device, window, compiler};
	auto compute_module = EX_PASS(
		VulkanShaderModule::compile(compiler, device, {{VShaderStage::compute, compute_shader}}));
	ctx.compute_pipe = EX_PASS(VulkanPipeline::create(device, {compute_module[0]}));

	int compute_size = 4 * 1024;
	vector<u32> compute_data(compute_size + 1, 0);
	compute_data[0] = compute_size;
	auto compute_usage =
		VBufferUsage::storage_buffer | VBufferUsage::transfer_dst | VBufferUsage::transfer_src;
	for(auto &buffer : ctx.compute_buffers)
		buffer = EX_PASS(VulkanBuffer::createAndUpload(*device, compute_data, compute_usage));
	ctx.font = EX_PASS(Font::makeDefault(device, window, 16));

	perf::ThreadContext perf_ctx;
	perf::Manager perf_mgr;
	perf::Analyzer perf_analyzer;
	ctx.perf_analyzer = &perf_analyzer;

	// TODO: sc_format might change if we move to different screen
	auto sc_format = ctx.device->swapChain()->format();
	auto gui_rpass = ctx.device->getRenderPass({{sc_format, 1}});
	Gui gui(device, window, gui_rpass, GuiConfig{GuiStyleMode::mini});
	ctx.gui = &gui;

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