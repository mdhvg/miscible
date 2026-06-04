#include "imgui.h"
#include "config.h"
#include "ui/theme.h"
#include "base/base_core.h"

MSCBL_EXP void restyle()
{
    ImGuiStyle &style = ImGui::GetStyle();
    style = ImGuiStyle(); // Reset layout

    // -------------------------------------------------------------------------
    // STRUCTURAL GEOMETRY & LAYOUT CONTROL
    // -------------------------------------------------------------------------
    style.Alpha = 1.0f;
    style.FontScaleDpi = 1.125f;

    // Window Spacing Configurations
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.ImageBorderSize = 1.0f;

    style.WindowMinSize = ImVec2(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING);
    style.FramePadding = ImVec2(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING);
    style.ItemInnerSpacing = ImVec2(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING);
    style.ItemSpacing = ImVec2(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING);
    style.IndentSpacing = MSCBL_INDENT;
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = SPACING(1.5f);

    style.WindowPadding = ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING);
    style.DisplayWindowPadding = ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING);
    style.DisplaySafeAreaPadding = ImVec2(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING);

    // Anti-Aliasing Rasterizer Toggles
    style.AntiAliasedLines = 1;
    style.AntiAliasedLinesUseTex = 1;
    style.AntiAliasedFill = 1;

    // Interaction Delays
    style.HoverStationaryDelay = 1.0f;

    // ----------------------------------------------------------------------
    // CORNER ROUNDING
    // ----------------------------------------------------------------------
    style.WindowRounding = 0.0f;    // RADIUS(0.5f);
    style.ChildRounding = 0.0f;     // RADIUS(0.5f);
    style.FrameRounding = 0.0f;     // RADIUS(0.5f);
    style.PopupRounding = 0.0f;     // RADIUS(0.5f);
    style.ScrollbarRounding = 0.0f; // RADIUS(0.5f);
    style.GrabRounding = 0.0f;      // RADIUS(0.5f);
    style.TabRounding = 0.0f;       // RADIUS(0.5f);

    // ----------------------------------------------------------------------
    // SHADCN-STYLE THEME MAPPING TO IMGUI COLORS
    // ----------------------------------------------------------------------

    // Core Background States
    style.Colors[ImGuiCol_WindowBg] = MSCBL_BACKGROUND;
    style.Colors[ImGuiCol_ChildBg] = MSCBL_SURFACE;
    style.Colors[ImGuiCol_PopupBg] = MSCBL_POPOVER;
    style.Colors[ImGuiCol_MenuBarBg] = MSCBL_BACKGROUND;

    // Borders & Separators
    style.Colors[ImGuiCol_Border] = MSCBL_BORDER;
    style.Colors[ImGuiCol_BorderShadow] = COLOR_TRANSPARENT;
    style.Colors[ImGuiCol_Separator] = MSCBL_BORDER;
    style.Colors[ImGuiCol_SeparatorHovered] = MSCBL_BORDER;
    style.Colors[ImGuiCol_SeparatorActive] = MSCBL_PRIMARY;

    // Typography
    style.Colors[ImGuiCol_Text] = MSCBL_FOREGROUND;
    style.Colors[ImGuiCol_TextDisabled] = MSCBL_FOREGROUND_MUTED;
    style.Colors[ImGuiCol_TextSelectedBg] = MSCBL_PRIMARY_HOVER;

    // Standard Form Fields (Checkboxes, Inputs, Sliders)
    style.Colors[ImGuiCol_FrameBg] = MSCBL_INTERACTION;
    style.Colors[ImGuiCol_FrameBgHovered] = MSCBL_INTERACTION_HOVER;
    style.Colors[ImGuiCol_FrameBgActive] = MSCBL_INTERACTION_ACTIVE;
    style.Colors[ImGuiCol_InputTextCursor] = MSCBL_PRIMARY;

    // Interactive Buttons
    style.Colors[ImGuiCol_Button] = MSCBL_INTERACTION;
    style.Colors[ImGuiCol_ButtonHovered] = MSCBL_INTERACTION_HOVER;
    style.Colors[ImGuiCol_ButtonActive] = MSCBL_INTERACTION_ACTIVE;

    // Window Header/Title Rails
    style.Colors[ImGuiCol_TitleBg] = MSCBL_BACKGROUND;
    style.Colors[ImGuiCol_TitleBgActive] = MSCBL_BACKGROUND_HOVER;
    style.Colors[ImGuiCol_TitleBgCollapsed] = MSCBL_BACKGROUND;

    // Complex Structural Hierarchies (TreeNodes, Menus, Selectables)
    style.Colors[ImGuiCol_Header] = MSCBL_INTERACTION;
    style.Colors[ImGuiCol_HeaderHovered] = MSCBL_INTERACTION_HOVER;
    style.Colors[ImGuiCol_HeaderActive] = MSCBL_INTERACTION_ACTIVE;

    // Scrollbars & Sliders Grabs
    style.Colors[ImGuiCol_ScrollbarBg] = MSCBL_BACKGROUND;
    style.Colors[ImGuiCol_ScrollbarGrab] = MSCBL_INTERACTION_HOVER;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = MSCBL_INTERACTION_ACTIVE;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = MSCBL_PRIMARY;
    style.Colors[ImGuiCol_CheckMark] = MSCBL_PRIMARY;
    style.Colors[ImGuiCol_SliderGrab] = MSCBL_INTERACTION_ACTIVE;
    style.Colors[ImGuiCol_SliderGrabActive] = MSCBL_PRIMARY;

    // Window Document Tab System Rails
    style.Colors[ImGuiCol_Tab] = MSCBL_BACKGROUND;
    style.Colors[ImGuiCol_TabHovered] = MSCBL_INTERACTION_HOVER;
    style.Colors[ImGuiCol_TabSelected] = MSCBL_SURFACE;
    style.Colors[ImGuiCol_TabSelectedOverline] = MSCBL_PRIMARY;
    style.Colors[ImGuiCol_TabDimmed] = MSCBL_BACKGROUND;
    style.Colors[ImGuiCol_TabDimmedSelected] = MSCBL_SURFACE;
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = MSCBL_PRIMARY;

    // Table Data Matrix Layouts
    style.Colors[ImGuiCol_TableHeaderBg] = MSCBL_BACKGROUND;
    style.Colors[ImGuiCol_TableBorderStrong] = MSCBL_BORDER;
    style.Colors[ImGuiCol_TableBorderLight] = MSCBL_BORDER_MUTED;
    style.Colors[ImGuiCol_TableRowBg] = MSCBL_BACKGROUND;
    style.Colors[ImGuiCol_TableRowBgAlt] = MSCBL_SURFACE;

    // Diagnostic Graph Metrics
    style.Colors[ImGuiCol_PlotLines] = MSCBL_PRIMARY;
    style.Colors[ImGuiCol_PlotLinesHovered] = MSCBL_FOREGROUND;
    style.Colors[ImGuiCol_PlotHistogram] = MSCBL_PRIMARY;
    style.Colors[ImGuiCol_PlotHistogramHovered] = MSCBL_INTERACTION_ACTIVE;

    // Global Overlays & Anchors
    style.Colors[ImGuiCol_ResizeGrip] = COLOR_TRANSPARENT; // Hide sizing corner dots
    style.Colors[ImGuiCol_ResizeGripHovered] = MSCBL_PRIMARY_HOVER;
    style.Colors[ImGuiCol_ResizeGripActive] = MSCBL_PRIMARY;
    style.Colors[ImGuiCol_DragDropTarget] = MSCBL_PRIMARY;
    style.Colors[ImGuiCol_DragDropTargetBg] = MSCBL_PRIMARY_HOVER;
    style.Colors[ImGuiCol_UnsavedMarker] = MSCBL_PRIMARY;
    style.Colors[ImGuiCol_NavCursor] = MSCBL_PRIMARY;
    style.Colors[ImGuiCol_NavWindowingHighlight] = MSCBL_FOREGROUND;
    style.Colors[ImGuiCol_NavWindowingDimBg] = MSCBL_POPOVER;
    style.Colors[ImGuiCol_ModalWindowDimBg] = RGBA255(10, 11, 12, 180);
}
