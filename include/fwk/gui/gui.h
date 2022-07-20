// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/format.h"
#include "fwk/gfx_base.h"
#include "fwk/maybe.h"
#include "fwk/vector.h"

#include "fwk/vulkan/vulkan_device.h"

namespace fwk {

DEFINE_ENUM(GuiStyleMode, normal, mini);

struct GuiConfig {
	GuiStyleMode style_mode = GuiStyleMode::normal;
	Maybe<string> font_path = none;
	Maybe<int> font_size = none;
};

// Allows creation of OpenGL-based user interfaces
// Currently based on ImGUI. Because of that only single instance can be created.
class Gui {
  public:
	Gui(VDeviceRef, VWindowRef, PVRenderPass, GuiConfig = {});
	Gui(Gui &&);
	~Gui();

	void operator=(const Gui &) = delete;

	// ---------- gui system control (gui.cpp) ------------------------------------------

	static bool isPresent();
	static Gui &instance();

	// All gui generating functions must be called between beginFrame & endFrame
	// After finishFrame input is filtered, and gui is ready to be drawn; Afterwards
	// either drawFrame or endFrame (or both) must be called
	void beginFrame(VulkanWindow &);
	vector<InputEvent> finishFrame(VulkanWindow &);

	void drawFrame(VulkanWindow &, VkCommandBuffer);
	void endFrame();

	AnyConfig config() const;
	void setConfig(const AnyConfig &);

	// Process in this context is a simple function which will be called at the end of beginFrame()
	using ProcessFunc = void (*)(void *);
	void addProcess(ProcessFunc, void *arg);
	void removeProcess(ProcessFunc, void *arg);

	float dpiScale() const { return m_dpi_scale; }

	bool o_hide = false;

	// ---------- widgets (widgets.cpp, widgets.h) --------------------------------------

	bool inputValue(const char *label, int &value);
	bool inputValue(const char *label, float &value);
	bool inputValue(const char *label, double &value);

	template <class Index> bool selectIndex(ZStr title, Index &value, CSpan<const char *> strings);
	template <class TEnum> bool selectFlags(EnumFlags<TEnum> &flag, CSpan<const char *> strings);

	template <class TEnum> bool selectFlags(EnumFlags<TEnum> &flag);
	template <class T> bool inputValue(ZStr title, T &value);

	template <class Enum, EnableIfEnum<Enum>...> bool selectEnum(ZStr title, Enum &value);

	template <class EType, class Index, class GetFunc, class SetFunc, EnableIfEnum<EType>...>
	void modifyEnums(ZStr title, const vector<Index> &selection, const GetFunc &get_func,
					 const SetFunc &set_func);

	template <class Type, class Index, class GetFunc, class SetFunc>
	bool modifyValues(ZStr title, const vector<Index> &selection, const GetFunc &get_func,
					  const SetFunc &set_func, bool on_enter = false);

	void text(Str);
	void centeredText(int center_pos, Str);

	template <c_formattible... T> void text(const char *str, T &&...args);
	template <c_formattible... T> void centeredText(int center_pos, const char *str, T &&...args);

	// ---------- popups (popups.cpp) ---------------------------------------------------

	using NameFilter = bool (*)(ZStr name);

	void openFilePopup(string &file_name, ZStr popup_name, NameFilter file_name_filter = nullptr);
	void openFileButton(string &file_name, string popup_name,
						NameFilter file_name_filter = nullptr);

	void openErrorPopup(Error, ZStr title = "");
	void displayErrorPopup();

	void showTooltip(Str str);
	void showHelpMarker(Str, const char *marker = "(?)");

  private:
	void updateDpiAndFonts(VulkanWindow &, bool is_initial);
	void rescaleWindows(float scale);

	struct Impl;
	Dynamic<Impl> m_impl;
	float m_dpi_scale = 1.0f;
	double m_last_time = -1.0;
	static Gui *s_instance;
};
}
