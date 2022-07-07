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

// TODO: what should we do when error happens in begin/end frame?
// TODO: proprly handle multiple queues
// TODO: proprly differentiate between graphics, present & compute queues

// TODO: handling VkResults
// TODO: making sure that handles are correct ?
// TODO: more type safety
// TODO: VHandle<> class ? handles are not initialized to null by default ...
// TODO: test_vulkan na nvidii jest ca�y czas wolniejszy ni� test_window (1300 vs 2220 FPS)
// TODO: mo�na jeszcze pewnie zrobi� du�o r�nego rodzaju optymalizacji, ale najpierw przyda�yby
//       si� jakie� konkretne programy testowe
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
	Maybe<FontCore> font_core;
	PVImage font_image;
	PVImageView font_image_view;
	PVSampler sampler;
	array<DescriptorSet, 2> descr_sets;
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
	setup.render_pass = render_graph.defaultRenderPass();
	setup.viewport = {IRect(0, 0, extent.width, extent.height)};
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

struct DrawRect {
	FRect rect;
	IColor fill_color;
	IColor border_color;
};

Ex<void> drawFrame(VulkanContext &ctx, CSpan<DrawRect> rects) {
	EXPECT(ctx.device->beginFrame());

	vector<MyVertex> vertices;
	vertices.reserve(rects.size() * 6);

	float2 scale = float2(2.0) / float2(ctx.window->extent());
	for(auto &draw_rect : rects) {
		float3 color = FColor(draw_rect.fill_color).rgb();
		auto corners = transform(draw_rect.rect.corners(), [&](float2 corner) -> MyVertex {
			float2 pos = corner * scale - float2(1.0);
			return {.pos = pos, .color = color, .texCoord = {}};
		});
		corners[1].texCoord.y = corners[2].texCoord.y = 1.0;
		corners[2].texCoord.x = corners[3].texCoord.x = 1.0;

		Array<MyVertex, 6> cur_verts = {
			{corners[0], corners[2], corners[1], corners[0], corners[3], corners[2]}};
		insertBack(vertices, cur_verts);
	}

	// TODO: createVertexBuffer and upload command add considerble overhead in this program
	// (FPS drops from 2500 to 800); We should probably amortize memory allocations and map/unmap
	// in staging buffer across multiple frames? Also we should avoid resource recreation? Supposedly
	// it's a slow operation...

	auto vbuffer = EX_PASS(createVertexBuffer(ctx, vertices));
	auto &render_graph = ctx.device->renderGraph();
	EXPECT(render_graph << CmdUpload(vertices, vbuffer));

	UBOData ubo;
	ubo.saturation = 0.25 + sin(getTime()) * 0.25;
	auto ubuffer = EX_PASS(createUniformBuffer(ctx));
	EXPECT(render_graph << CmdUpload(cspan(&ubo, 1), ubuffer));

	auto &descr_set = ctx.descr_sets[render_graph.swapFrameIndex()];
	descr_set.update({{.type = VDescriptorType::uniform_buffer, .binding = 0, .data = ubuffer},
					  {.type = VDescriptorType::combined_image_sampler,
					   .binding = 1,
					   .data = std::make_pair(ctx.sampler, ctx.font_image_view)}});
	render_graph << CmdBindDescriptorSet{.index = 0, .set = &descr_set};

	VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	render_graph << CmdBeginRenderPass{.render_pass = ctx.pipeline->renderPass(),
									   .clear_values = {{clear_color}}};
	render_graph << CmdBindPipeline{ctx.pipeline};
	render_graph << CmdBindVertexBuffers({{vbuffer}}, vector<u64>{{0}});
	render_graph << CmdDraw{.num_vertices = vertices.size()};
	render_graph << CmdEndRenderPass();
	EXPECT(ctx.device->finishFrame());

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

	vector<DrawRect> rects;
	rects.reserve(positions.size());
	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect({-50, -50}, {50, 50}) + positions[n];
		FColor fill_color(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f);
		IColor border_color = ColorId::black;
		rects.emplace_back(rect, IColor(fill_color), border_color);
	}

	drawFrame(ctx, rects).check();

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

	return true;
}

Ex<PVImage> makeTexture(VulkanContext &ctx, const Image &image) {
	auto vimage =
		EX_PASS(VulkanImage::create(ctx.device, toVkExtent(image.size()),
									{.format = VK_FORMAT_R8G8B8A8_SRGB}, VMemoryUsage::device));
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
	auto window = EX_PASS(VulkanWindow::create(instance, "fwk::vulkan_test", IRect(0, 0, 1280, 720),
											   VulkanWindowConfig{flags}));

	VulkanDeviceSetup dev_setup;
	auto pref_device = instance->preferredDevice(window->surfaceHandle(), &dev_setup.queues);
	if(!pref_device)
		return ERROR("Couldn't find a suitable Vulkan device");
	auto device = EX_PASS(instance->createDevice(*pref_device, dev_setup));
	auto phys_info = instance->info(device->physId());
	print("Selected Vulkan physical device: %\nDriver version: %\n",
		  phys_info.properties.deviceName, phys_info.properties.driverVersion);

	auto swap_chain = EX_PASS(VulkanSwapChain::create(
		device, window, {.preferred_present_mode = VPresentMode::immediate}));
	EXPECT(device->createRenderGraph(swap_chain));

	VulkanContext ctx{device, window};
	EXPECT(createPipeline(ctx));

	int font_size = 16 * window->dpiScale();
	auto font_data = EX_PASS(FontFactory().makeFont(fontPath(), font_size));
	ctx.font_core = move(font_data.core);
	ctx.font_image = EX_PASS(makeTexture(ctx, font_data.image));
	ctx.font_image_view = EX_PASS(VulkanImageView::create(ctx.device, ctx.font_image));
	ctx.sampler = EX_PASS(ctx.device->createSampler({}));

	DescriptorPoolSetup pool_setup;
	pool_setup.sizes[VDescriptorType::uniform_buffer] = 2;
	pool_setup.sizes[VDescriptorType::combined_image_sampler] = 2;
	pool_setup.max_sets = 2;
	auto pool = EX_PASS(ctx.device->createDescriptorPool(pool_setup));
	for(auto &set : ctx.descr_sets)
		set = EX_PASS(pool->alloc(ctx.pipeline->pipelineLayout(), 0));
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
