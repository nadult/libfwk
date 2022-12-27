// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/gui/gui.h"
#include "fwk/gui/imgui.h"
#include "fwk/span.h"

namespace fwk {

template <class Index>
bool Gui::selectIndex(ZStr title, Index &value, CSpan<const char *> strings) {
	DASSERT(strings.inRange((int)value));

	ImGui::Text("%s", title.c_str());
	int width = ImGui::GetItemRectSize().x;
	ImGui::SameLine();
	ImGui::PushItemWidth(220 * dpiScale() - width);
	int item = (int)value;
	bool ret = ImGui::Combo(format("##%", title).c_str(), &item, strings.data(), strings.size());
	ImGui::PopItemWidth();

	value = (Index)item;
	return ret;
}

template <class TEnum> bool Gui::selectFlags(EnumFlags<TEnum> &flag, CSpan<const char *> strings) {
	DASSERT(strings.size() == count<TEnum>);
	bool changed = false;

	for(auto opt : all<TEnum>) {
		bool enabled = flag & opt;
		if(ImGui::Checkbox(strings[(int)opt], &enabled)) {
			flag = (flag & ~opt) | mask(enabled, opt);
			changed = true;
		}
	}

	return changed;
}

template <class TEnum> bool Gui::selectFlags(EnumFlags<TEnum> &flag) {
	array<const char *, count<TEnum>> names;
	for(auto item : all<TEnum>)
		names[int(item)] = toString(item);
	return selectFlags(flag, names);
}

template <class T> bool Gui::inputValue(ZStr title, T &value) {
	TextFormatter tmp;
	tmp << "##";
	tmp << title;

	ImGui::Text("%s", title.c_str());
	int width = ImGui::GetItemRectSize().x;
	ImGui::SameLine();
	ImGui::PushItemWidth(220 * dpiScale() - width);
	bool changed = inputValue(tmp.c_str(), value);
	ImGui::PopItemWidth();
	return changed;
}

template <class Enum, EnableIfEnum<Enum>...> bool Gui::selectEnum(ZStr title, Enum &value) {
	array<const char *, count<Enum>> strings;
	for(auto val : all<Enum>)
		strings[(int)val] = toString(val);
	return selectIndex(title, value, strings);
}

template <class EType, class Index, class GetFunc, class SetFunc, EnableIfEnum<EType>...>
void Gui::modifyEnums(ZStr title, const vector<Index> &selection, const GetFunc &get_func,
					  const SetFunc &set_func) {
	if(!selection)
		return;

	bool all_same = true;
	EType first = get_func(selection[0]);
	for(int n = 1; n < selection.size(); n++)
		if(get_func(selection[n]) != first) {
			all_same = false;
			break;
		}

	array<const char *, count<EType> + 1> items;
	items[0] = "...";
	for(auto val : all<EType>)
		items[int(val) + 1] = toString(val);
	int value = all_same ? int(first) + 1 : 0;
	if(selectIndex(title, value, items) && value != 0) {
		for(auto id : selection)
			set_func(id, EType(value - 1));
	}
}

template <class Type, class Index, class GetFunc, class SetFunc>
bool Gui::modifyValues(ZStr title, const vector<Index> &selection, const GetFunc &get_func,
					   const SetFunc &set_func, bool on_enter) {
	if(!selection)
		return false;

	bool all_same = true;
	Type first = get_func(selection[0]);
	for(int n = 1; n < selection.size(); n++)
		if(get_func(selection[n]) != first) {
			all_same = false;
			break;
		}

	string value = all_same ? toString(first) : "...";
	char buffer[1024];
	snprintf(buffer, arraySize(buffer), "%s", value.c_str());
	ImGui::Text("%s", title.c_str());
	ImGui::SameLine();
	uint flags = on_enter ? ImGuiInputTextFlags_EnterReturnsTrue : 0;
	if(ImGui::InputText(format("##_%", title).c_str(), buffer, arraySize(buffer), flags)) {
		for(auto id : selection)
			set_func(id, buffer);
		return true;
	}
	return false;
}

template <c_formattible... T> void Gui::text(const char *str, T &&...args) {
	TextFormatter fmt(256, {FormatMode::structured});
	fmt(str, std::forward<T>(args)...);
	text(fmt.text());
}

template <c_formattible... T> void Gui::centeredText(int center_pos, const char *str, T &&...args) {
	TextFormatter fmt;
	fmt(str, std::forward<T>(args)...);
	centeredText(center_pos, fmt.text());
}
}
