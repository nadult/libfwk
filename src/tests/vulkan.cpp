// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/format.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/font_factory.h>
#include <fwk/gfx/font_finder.h>
#include <fwk/gfx/gl_texture.h>
#include <fwk/gfx/renderer2d.h>
#include <fwk/gfx/shader_compiler.h>
#include <fwk/io/file_system.h>
#include <fwk/sys/expected.h>
#include <fwk/sys/input.h>
#include <fwk/vulkan/vulkan_buffer.h>
#include <fwk/vulkan/vulkan_device.h>
#include <fwk/vulkan/vulkan_framebuffer.h>
#include <fwk/vulkan/vulkan_image.h>
#include <fwk/vulkan/vulkan_instance.h>
#include <fwk/vulkan/vulkan_pipeline.h>
#include <fwk/vulkan/vulkan_render_graph.h>
#include <fwk/vulkan/vulkan_swap_chain.h>
#include <fwk/vulkan/vulkan_window.h>

#include <vulkan/vulkan.h>

// TODO: handling VkResults
// TODO: making sure that handles are correct ?
// TODO: more type safety
// TODO: VHandle<> class ? handles are not initialized to null by default ...
// TODO: kolejnoœæ niszczenia obiektów...

using namespace fwk;

const char *vertex_shader = R"(
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
	gl_Position = vec4(inPosition, 0.0, 1.0);
	fragColor = inColor;
}
)";

const char *fragment_shader = R"(
#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;

void main() {
	outColor = vec4(fragColor, 1.0);
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

	static void addStreamDesc(vector<VertexBindingDesc> &bindings,
							  vector<VertexAttribDesc> &attribs, int index, int first_location) {
		bindings.emplace_back(VertexBindingDesc{.index = u8(index), .stride = sizeof(MyVertex)});
		attribs.emplace_back(VertexAttribDesc{
			.format = VkFormat::VK_FORMAT_R32G32_SFLOAT,
			.offset = 0,
			.location_index = u8(first_location),
			.binding_index = u8(index),
		});
		attribs.emplace_back(VertexAttribDesc{
			.format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
			.offset = sizeof(pos),
			.location_index = u8(first_location + 1),
			.binding_index = u8(index),
		});
	}
};

struct VulkanContext {
	VDeviceRef device;
	VWindowRef window;
	PVPipeline pipeline;
	Maybe<Font> font;
};

Ex<void> createPipeline(VulkanContext &ctx) {
	// TODO: making sure that shaderc_shared.dll is available
	ShaderCompiler compiler;
	auto vsh_code = compiler.compile(ShaderType::vertex, vertex_shader);
	auto fsh_code = compiler.compile(ShaderType::fragment, fragment_shader);

	if(!vsh_code.bytecode)
		return ERROR("Failed to compile vertex shader:\n%", vsh_code.messages);
	if(!fsh_code.bytecode)
		return ERROR("Failed to compile fragment shader:\n%", fsh_code.messages);

	auto vsh_module = EX_PASS(ctx.device->createShaderModule(vsh_code.bytecode));
	auto fsh_module = EX_PASS(ctx.device->createShaderModule(fsh_code.bytecode));

	auto &render_graph = ctx.device->renderGraph();
	auto swap_chain = render_graph.swapChain();
	auto sc_image = swap_chain->imageViews().front()->image();
	auto extent = sc_image->extent();
	VPipelineSetup setup;
	setup.stages.emplace_back(vsh_module, VShaderStage::vertex);
	setup.stages.emplace_back(fsh_module, VShaderStage::fragment);
	setup.viewport = {.x = 0.0f,
					  .y = 0.0f,
					  .width = (float)extent.width,
					  .height = (float)extent.height,
					  .minDepth = 0.0f,
					  .maxDepth = 1.0f};
	setup.scissor = {.offset = {0, 0}, .extent = extent};
	setup.render_pass = render_graph.defaultRenderPass();
	MyVertex::addStreamDesc(setup.vertex_bindings, setup.vertex_attribs, 0, 0);

	ctx.pipeline = EX_PASS(VulkanPipeline::create(ctx.device, setup));
	return {};
}

Ex<PVBuffer> createVertexBuffer(VulkanContext &ctx, CSpan<MyVertex> vertices) {
	auto usage = VBufferUsageFlag::vertex_buffer | VBufferUsageFlag::transfer_dst;
	auto buffer = EX_PASS(VulkanBuffer::create<MyVertex>(ctx.device, vertices.size(), usage));
	auto mem_req = buffer->memoryRequirements();
	auto mem_flags = VMemoryFlag::device_local;
	auto memory =
		EX_PASS(ctx.device->allocDeviceMemory(mem_req.size, mem_req.memoryTypeBits, mem_flags));
	EXPECT(buffer->bindMemory(memory));
	return buffer;
}

Ex<void> drawFrame(VulkanContext &ctx) {
	auto &render_graph = ctx.device->renderGraph();

	EXPECT(render_graph.beginFrame());

	vector<MyVertex> vertices = {{{0.0f, -0.75f}, {1.0f, 0.0f, 0.0f}},
								 {{0.75f, 0.75f}, {0.0f, 1.0f, 0.0f}},
								 {{-0.75f, 0.75f}, {0.0f, 0.0f, 1.0f}}};

	auto vbuffer = EX_PASS(createVertexBuffer(ctx, vertices));
	render_graph << CmdUpload(vertices, vbuffer);

	VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	render_graph << CmdBeginRenderPass{.render_pass = ctx.pipeline->renderPass(),
									   .clear_values = {{clear_color}}};
	render_graph << CmdBindPipeline{ctx.pipeline};
	render_graph << CmdBindVertexBuffers{.buffers{{vbuffer}}, .offsets{{0}}};
	render_graph << CmdDraw{.num_vertices = vertices.size()};
	render_graph << CmdEndRenderPass();
	EXPECT(render_graph.finishFrame());

	return {};
}

void updateFPS(VulkanWindow &window) {
	static double prev_time = getTime();
	static int num_frames = 0;
	double cur_time = getTime();
	num_frames++;
	if(cur_time - prev_time > 1.0) {
		int fps = double(num_frames) / (cur_time - prev_time);
		window.setTitle(format("fwk::vulkan_test [FPS: %]", fps));
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

	drawFrame(ctx).check();

	updateFPS(window);

	//clearColor(IColor(50, 0, 50));
	/*Renderer2D renderer(IRect(VkDevice::instance().windowSize()), Orient2D::y_down);

	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect({-50, -50}, {50, 50}) + positions[n];
		FColor fill_color(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f);
		IColor border_color = ColorId::black;

		renderer.addFilledRect(rect, fill_color);
		renderer.addRect(rect, border_color);
	}

	auto text = format("Hello world!\nWindow size: %", device.windowSize());
	font.draw(renderer, FRect({5, 5}, {200, 20}), {ColorId::white}, text);
	renderer.render();*/

	// TODO: is this a good place ?
	ctx.device->nextReleasePhase();

	return true;
}

Ex<int> exMain() {
	VulkanInstanceSetup setup;
	setup.debug_levels = VDebugLevel::warning | VDebugLevel::error;
	setup.debug_types = all<VDebugType>;
	auto instance = EX_PASS(VulkanInstance::create(setup));

	auto flags = VWindowFlag::resizable | VWindowFlag::vsync | VWindowFlag::centered |
				 VWindowFlag::allow_hidpi;
	auto window = EX_PASS(VulkanWindow::create(instance, "fwk::vulkan_test", IRect(0, 0, 1280, 720),
											   VulkanWindowConfig{flags}));

	VulkanDeviceSetup dev_setup;
	auto pref_device = instance->preferredDevice(window->surfaceHandle(), &dev_setup.queues);
	if(!pref_device)
		return ERROR("Couldn't find a suitable Vulkan device");
	auto device = EX_PASS(instance->createDevice(*pref_device, dev_setup));

	auto surf_info = VulkanSwapChain::surfaceInfo(device, window);
	auto swap_chain =
		EX_PASS(VulkanSwapChain::create(device, window,
										{.surface_format = surf_info.formats[0],
										 .present_mode = surf_info.present_modes[0],
										 .transform = surf_info.capabilities.currentTransform}));
	EXPECT(device->createRenderGraph(swap_chain));

	VulkanContext ctx{device, window};
	EXPECT(createPipeline(ctx));

	int font_size = 16 * window->dpiScale();
	//ctx.font = FontFactory().makeFont(fontPath(), font_size);
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
