// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/investigate.h"

#include "fwk/any.h"
#include "fwk/gfx/gl_device.h"
#include "fwk/gfx/investigator2.h"
#include "fwk/gfx/investigator3.h"
#include "fwk/gfx/visualizer2.h"
#include "fwk/gfx/visualizer3.h"
#include "fwk/perf_base.h"

namespace fwk {

static Dynamic<GlDevice> spawnGlDevice() {
	if(GlDevice::isPresent())
		return {};
	Dynamic<GlDevice> device;
	device.emplace();
	device->createWindow("Frontier v0.04", int2(1024, 768),
						 GlDeviceOpt::resizable | GlDeviceOpt::vsync);
	return device;
}

void investigate(VisFunc2 vis_func, Maybe<DRect> focus, InvestigatorOpts flags) {
	PERF_SCOPE();
	auto device = spawnGlDevice();
	Investigator2 rutkowski(vis_func, focus, flags);
	rutkowski.run();
}

void investigate(VisFunc3 vis_func, Maybe<DBox> focus, InvestigatorOpts flags) {
	PERF_SCOPE();
	auto device = spawnGlDevice();
	Investigator3 sherlock(vis_func, focus, flags);
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
