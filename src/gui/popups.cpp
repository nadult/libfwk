// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gui/gui.h"

#include "fwk/algorithm.h"
#include "fwk/format.h"
#include "fwk/gui/imgui.h"
#include "fwk/index_range.h"
#include "fwk/io/file_system.h"
#include "fwk/sys/error.h"
#include "fwk/sys/expected.h"
#include "fwk/vector.h"
#include "gui_impl.h"

namespace fwk {
void Gui::openErrorPopup(Error error, ZStr title) {
	if(!error.empty()) {
		m_impl->error_popup = toString(error);
		m_impl->error_popup_title = string(title) + "##error_popup";
		ImGui::OpenPopup(m_impl->error_popup_title.c_str());
	}
}

void Gui::displayErrorPopup() {
	if(ImGui::BeginPopupModal(m_impl->error_popup_title.c_str(), nullptr,
							  ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("%s", m_impl->error_popup.c_str());
		ImGui::Separator();

		int enter_idx = ImGui::GetKeyIndex(ImGuiKey_Enter);
		if(ImGui::Button("OK", ImVec2(120, 0)) || ImGui::IsKeyDown(enter_idx)) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

template <class... Args>
static GuiPopupContext &findContext(vector<Pair<string, GuiPopupContext>> &contexts,
									string context_name, Args &&...init_args) {
	for(auto &pair : contexts)
		if(pair.first == context_name)
			return pair.second;

	contexts.emplace_back(context_name, GuiPopupContext(std::forward<Args>(init_args)...));
	return contexts.back().second;
}

static void dropContext(vector<Pair<string, GuiPopupContext>> &contexts, string context_name) {
	for(int idx : intRange(contexts))
		if(contexts[idx].first == context_name)
			contexts.erase(contexts.begin() + idx);
}

void Gui::openFilePopup(string &file_name, ZStr popup_name, NameFilter name_filter) {
	if(ImGui::BeginPopup(popup_name.c_str())) {
		auto &ctx = findContext(m_impl->popup_contexts, popup_name, file_name, name_filter);

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
			if(entry.is_dir || (ctx.name_filter && !ctx.name_filter(entry.path)))
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
			dropContext(m_impl->popup_contexts, popup_name);
		ImGui::EndPopup();
	}
}

void Gui::openFileButton(string &file_path_str, string popup_name, NameFilter name_filter) {
	popup_name += "_open_file";
	FilePath file_path(file_path_str);
	if(file_path.isAbsolute())
		file_path = file_path.relativeToCurrent().get();
	file_path_str = file_path;

	if(ImGui::Button(format("File: %", file_path_str).c_str()))
		ImGui::OpenPopup(popup_name.c_str());
	openFilePopup(file_path_str, popup_name, name_filter);
}

void Gui::showTooltip(Str str) {
	ImGui::BeginTooltip();
	ImGui::PushTextWrapPos(450.0f);
	text(str);
	ImGui::PopTextWrapPos();
	ImGui::EndTooltip();
}

void Gui::showHelpMarker(Str text, const char *marker) {
	ImGui::TextDisabled("%s", marker);
	if(ImGui::IsItemHovered())
		showTooltip(text);
}
}