#pragma once
#include "UI/Theme.h"

#define BUTTON_DEFAULT(...)                                                        \
	{                                                                              \
		ImGui::PushStyleColor(ImGuiCol_Button, DARK_PRIMARY);                      \
		ImGui::PushStyleColor(ImGuiCol_Text, DARK_PRIMARY_FOREGROUND);             \
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, DARK_PRIMARY);                \
		ImVec4 temp_color = DARK_PRIMARY;                                          \
		temp_color.w = 0.9;                                                        \
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, temp_color);                 \
                                                                                   \
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {SPACING(4), SPACING(2)}); \
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RADIUS(1) - 2);           \
		__VA_ARGS__                                                                \
		ImGui::PopStyleColor(4);                                                   \
		ImGui::PopStyleVar(2);                                                     \
	}

#define BUTTON_OUTLINE(...)                                                        \
	{                                                                              \
		ImVec4 temp_color = DARK_INPUT;                                            \
		temp_color.w = 0.3;                                                        \
		ImGui::PushStyleColor(ImGuiCol_Button, temp_color);                        \
		temp_color.w = 0.5;                                                        \
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, temp_color);                  \
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, temp_color);                 \
		ImGui::PushStyleColor(ImGuiCol_Border, DARK_INPUT);                        \
                                                                                   \
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {SPACING(4), SPACING(2)}); \
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);                     \
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RADIUS(1) - 2);           \
		__VA_ARGS__                                                                \
		ImGui::PopStyleColor(4);                                                   \
		ImGui::PopStyleVar(3);                                                     \
	}

#define BUTTON_SECONDARY(...)                                                      \
	{                                                                              \
		ImGui::PushStyleColor(ImGuiCol_Button, DARK_SECONDARY);                    \
		ImGui::PushStyleColor(ImGuiCol_Text, DARK_SECONDARY_FOREGROUND);           \
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, DARK_SECONDARY);              \
		ImVec4 temp_color = DARK_SECONDARY;                                        \
		temp_color.w = 0.8;                                                        \
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, temp_color);                 \
                                                                                   \
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {SPACING(4), SPACING(2)}); \
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RADIUS(1) - 2);           \
		__VA_ARGS__                                                                \
		ImGui::PopStyleColor(4);                                                   \
		ImGui::PopStyleVar(2);                                                     \
	}

#define BUTTON_GHOST(...)                                                          \
	{                                                                              \
		ImVec4 temp_color = DARK_ACCENT;                                           \
		temp_color.w = 0.5;                                                        \
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, temp_color);                  \
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, temp_color);                 \
                                                                                   \
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {SPACING(4), SPACING(2)}); \
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RADIUS(1) - 2);           \
		__VA_ARGS__                                                                \
		ImGui::PopStyleColor(2);                                                   \
		ImGui::PopStyleVar(2);                                                     \
	}