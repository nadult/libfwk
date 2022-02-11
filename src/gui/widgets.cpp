// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gui/gui.h"

#include "fwk/gui/imgui.h"
#include "gui_impl.h"

namespace fwk {

bool Gui::inputValue(const char *label, int &value) { return ImGui::InputInt(label, &value); }
bool Gui::inputValue(const char *label, float &value) { return ImGui::InputFloat(label, &value); }
bool Gui::inputValue(const char *label, double &value) { return ImGui::InputDouble(label, &value); }

void Gui::centeredText(int center_pos, Str str) {
	auto tsize = ImGui::CalcTextSize(str.begin(), str.end());
	ImGui::Dummy({center_pos - tsize.x * 0.5f, 1.0f});
	ImGui::SameLine();
	ImGui::TextUnformatted(str.begin(), str.end());
}

void Gui::text(Str str) { ImGui::TextUnformatted(str.begin(), str.end()); }
}
