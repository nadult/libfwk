// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gui/gui.h"
#include "fwk/io/file_system.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct GuiPopupContext {
	GuiPopupContext(string file_name, Gui::NameFilter name_filter)
		: current_file(move(file_name)), name_filter(name_filter) {
		FilePath fpath(current_file);
		current_dir = fpath.isDirectory() ? fpath : fpath.parent();
	}

	FilePath current_file;
	FilePath current_dir;
	Gui::NameFilter name_filter;
	bool show_hidden = false;
};

struct Gui::Impl {
	struct Process {
		ProcessFunc func;
		void *arg;
	};

	VDeviceRef device;
	VkDescriptorPool descr_pool = nullptr;

	int font_size;
	string font_path;
	string error_popup, error_popup_title;
	vector<pair<string, GuiPopupContext>> popup_contexts;
	vector<Process> processes;
	bool fonts_initialized = false;
};
}