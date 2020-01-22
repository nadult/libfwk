// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/menu_imgui.h"

#include "fwk/algorithm.h"
#include "fwk/format.h"
#include "fwk/index_range.h"
#include "fwk/io/file_system.h"
#include "fwk/sys/expected.h"
#include "fwk/vector.h"
#include <regex>

namespace menu {

struct PopupContext {
	PopupContext(string file_name, string file_regex)
		: current_file(move(file_name)), regex(move(file_regex)) {
		FilePath fpath(current_file);
		current_dir = fpath.isDirectory() ? fpath : fpath.parent();
	}

	FilePath current_file;
	FilePath current_dir;
	std::regex regex;
	bool show_hidden = false;
};

template <class Context, class... Args>
PopupContext &findContext(vector<pair<string, Context>> &contexts, string context_name,
						  Args &&... init_args) {
	for(auto &pair : contexts)
		if(pair.first == context_name)
			return pair.second;

	contexts.emplace_back(context_name, Context(std::forward<Args>(init_args)...));
	return contexts.back().second;
}

template <class Context, class... Args>
void dropContext(vector<pair<string, Context>> &contexts, string context_name) {
	for(int idx : intRange(contexts))
		if(contexts[idx].first == context_name)
			contexts.erase(contexts.begin() + idx);
}

void openFilePopup(string &file_name, ZStr popup_name, string file_regex) {
	static vector<pair<string, PopupContext>> contexts;

	if(ImGui::BeginPopup(popup_name.c_str())) {
		auto &ctx = findContext(contexts, popup_name, file_name, file_regex);

		// TODO: dir is broken
		Maybe<string> new_dir;

		ImGui::Text("%s", ctx.current_dir.c_str());
		ImGui::Separator();

		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 60), ImVec2(400, 400));
		ImGui::BeginChild("", ImVec2(0, 0), 0,
						  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

		auto opts = FindFileOpt::regular_file | FindFileOpt::directory | FindFileOpt::relative;
		auto entries = findFiles(ctx.current_dir, opts);

		bool drop_context = false;

		makeSorted(entries);
		if(!ctx.show_hidden)
			entries = filter(entries,
							 [](const FileEntry &entry) { return entry.path.c_str()[0] != '.'; });

		if(!ctx.current_dir.isRoot())
			entries.insert(entries.begin(), {"..", true, false});

		for(auto &entry : entries) {
			if(entry.is_dir &&
			   ImGui::Selectable(entry.path.c_str(), false, ImGuiSelectableFlags_DontClosePopups))
				new_dir = ctx.current_dir / entry.path;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(200, 255, 200, 255));
		for(auto &entry : entries) {
			std::cmatch match;
			if(entry.is_dir || !std::regex_match(entry.path.c_str(), ctx.regex))
				continue;

			FilePath absolute = ctx.current_dir / entry.path;
			if(ImGui::Selectable(entry.path.c_str(), absolute == ctx.current_file)) {
				ImGui::CloseCurrentPopup();
				drop_context = true;
				file_name = absolute;
			}
		}
		ImGui::PopStyleColor(1);

		ImGui::EndChild();

		ImGui::Separator();
		ImGui::Checkbox("Show hidden", &ctx.show_hidden);
		ImGui::SameLine(200.0f);
		if(ImGui::Button("cancel")) {
			ImGui::CloseCurrentPopup();
			drop_context = true;
		}

		if(new_dir)
			ctx.current_dir = *new_dir;

		if(drop_context)
			dropContext(contexts, popup_name);

		ImGui::EndPopup();
	}
}

void openFileButton(string &file_path_str, string popup_name, string regex) {
	popup_name += "_open_file";
	FilePath file_path(file_path_str);
	if(file_path.isAbsolute())
		file_path = file_path.relativeToCurrent().get();
	file_path_str = file_path;

	if(ImGui::Button(format("File: %", file_path_str).c_str()))
		ImGui::OpenPopup(popup_name.c_str());
	menu::openFilePopup(file_path_str, popup_name, regex);
}
}
