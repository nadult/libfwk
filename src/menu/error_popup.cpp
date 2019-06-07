// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/menu_imgui.h"

#include <fwk/sys/error.h>

namespace menu {

static Error s_current_error;

void openErrorPopup(Error error) {
	if(!error.empty()) {
		s_current_error = move(error);
		ImGui::OpenPopup("error_popup");
	}
}

void displayErrorPopup() {
	if(ImGui::BeginPopupModal("error_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("%s", toString(s_current_error).c_str());
		ImGui::Separator();

		int enter_idx = ImGui::GetKeyIndex(ImGuiKey_Enter);
		if(ImGui::Button("OK", ImVec2(120, 0)) || ImGui::IsKeyDown(enter_idx)) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
}
