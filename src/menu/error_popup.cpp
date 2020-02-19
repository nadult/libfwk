// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/menu_imgui.h"

#include <fwk/sys/error.h>

namespace menu {

static string s_current_error, s_title;

void openErrorPopup(Error error, ZStr title) {
	if(!error.empty()) {
		s_current_error = toString(error);
		s_title = string(title) + "##error_popup";
		ImGui::OpenPopup(s_title.c_str());
	}
}

void displayErrorPopup() {
	if(ImGui::BeginPopupModal(s_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("%s", s_current_error.c_str());
		ImGui::Separator();

		int enter_idx = ImGui::GetKeyIndex(ImGuiKey_Enter);
		if(ImGui::Button("OK", ImVec2(120, 0)) || ImGui::IsKeyDown(enter_idx)) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
}
