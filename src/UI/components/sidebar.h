#pragma once
#include "UI/Theme.h"

#define SIDEBAR(...)                                                               \
	{                                                                              \
		ImGui::PushStyleColor(ImGuiCol_WindowBg, DARK_SIDEBAR);                    \
		ImGui::PushStyleColor(ImGuiCol_ChildBg, DARK_SIDEBAR);                     \
		ImGui::PushStyleColor(ImGuiCol_FrameBg, DARK_SIDEBAR);                     \
		ImGui::PushStyleColor(ImGuiCol_Button, DARK_SIDEBAR);                      \
                                                                                   \
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {SPACING(4), SPACING(2)}); \
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RADIUS(1) - 2);           \
		__VA_ARGS__                                                                \
		ImGui::PopStyleColor(4);                                                   \
		ImGui::PopStyleVar(2);                                                     \
	}