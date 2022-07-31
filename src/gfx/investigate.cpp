// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/investigate.h"

#include "fwk/any.h"
#include "fwk/gfx/investigator2.h"
#include "fwk/gfx/investigator3.h"
#include "fwk/gfx/visualizer2.h"
#include "fwk/gfx/visualizer3.h"
#include "fwk/perf_base.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_window.h"

namespace fwk {

struct VulkanContext {
	VDeviceRef device;
	VWindowRef window;
};

static Ex<VulkanContext> spawnVulkanDevice() {
	auto instance =
		VulkanInstance::isPresent() ? VulkanInstance::ref() : EX_PASS(VulkanInstance::create({}));
	auto flags = VWindowFlag::resizable | VWindowFlag::centered | VWindowFlag::allow_hidpi;
	auto window = EX_PASS(
		VulkanWindow::create(instance, "libfwk investigator", IRect(0, 0, 1280, 720), flags));

	VDeviceSetup dev_setup;
	auto pref_device = instance->preferredDevice(window->surfaceHandle(), &dev_setup.queues);
	if(!pref_device)
		return ERROR("Couldn't find a suitable Vulkan device");
	auto device = EX_PASS(instance->createDevice(*pref_device, dev_setup));
	EXPECT(device->addSwapChain(window));

	// TODO: use existing window?
	return VulkanContext{device, window};
}

void investigate(VisFunc2 vis_func, Maybe<DRect> focus, InvestigatorOpts flags) {
	PERF_SCOPE();
	auto context = spawnVulkanDevice().get();
	Investigator2 rutkowski(context.device, context.window, vis_func, focus, flags);
	rutkowski.run();
}

void investigate(VisFunc3 vis_func, Maybe<DBox> focus, InvestigatorOpts flags) {
	PERF_SCOPE();
	auto context = spawnVulkanDevice().get();
	Investigator3 sherlock(context.device, context.window, vis_func, flags, {focus});
	sherlock.run();
}

void investigateOnFail(const Expected<void> &expected) {
	PERF_SCOPE();
	if(expected)
		return;

	for(auto &val : expected.error().values) {
		if(const VisFunc2 *vis2 = val) {
			auto func = [&](Visualizer2 &vis, double2 mouse_pos) -> string {
				auto result = (*vis2)(vis, mouse_pos);
				return format("%\n%", result, expected.error());
			};
			return investigate(func);
		} else if(const VisFunc3 *vis3 = val) {
			auto func = [&](Visualizer3 &vis, double2 mouse_pos) -> string {
				auto result = (*vis3)(vis, mouse_pos);
				return format("%\n%", result, expected.error());
			};
			return investigate(func);
		}
	}

	expected.check();
}
}
