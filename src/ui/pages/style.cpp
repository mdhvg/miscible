#include "imgui.h"
#include "ui/theme.h"

extern "C" void restyle()
{
    ImGuiStyle &style = ImGui::GetStyle();
    style             = ImGuiStyle();

    // Reset ALL styles
    style.Alpha                            = 1;                            // Global alpha applies to everything in Dear ImGui.
    style.WindowPadding                    = {0, 0};                       // Padding within a window.
    style.WindowRounding                   = {0};                          // Radius of window corners rounding. Set to 0.0f to have rectangular windows. Large values tend to lead to variety of artifacts and are not recommended.
    style.WindowBorderSize                 = 1;                            // Thickness of border around windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    style.WindowMinSize                    = ImVec2(1, 1);                 // Minimum window size. This is a global setting. If you want to constrain individual windows, use SetNextWindowSizeConstraints().
    style.ChildBorderSize                  = 1.0f;                         // Thickness of border around child windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    style.PopupRounding                    = {RADIUS(0.5) - 2};            // Radius of popup window corners rounding. (Note that tooltip windows use WindowRounding)
    style.PopupBorderSize                  = 1.0f;                         // Thickness of border around popup/tooltip windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    style.FramePadding                     = {SPACING(1.5), SPACING(1.5)}; // Padding within a framed rectangle (used by most widgets).
    style.FrameRounding                    = {RADIUS(0.5) - 2};            // Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most widgets).
    style.FrameBorderSize                  = 0.0f;                         // Thickness of border around frames. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
    style.ItemSpacing                      = ImVec2(0, 0);                 // Horizontal and vertical spacing between widgets/lines.
    style.ItemInnerSpacing                 = {SPACING(1.5), SPACING(2)};   // Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label).
    style.CellPadding                      = ImVec2();                     // Padding within a table cell. Cellpadding.x is locked for entire table. CellPadding.y may be altered between different rows.
    style.TouchExtraPadding                = ImVec2();                     // Expand reactive bounding box for touch-based system where touch position is not accurate enough. Unfortunately we don't sort widgets so priority on overlap will always be given to the first widget. So don't grow this too much!
    style.IndentSpacing                    = {0};                          // Horizontal indentation when e.g. entering a tree node. Generally == (FontSize + FramePadding.x*2).
    style.ColumnsMinSpacing                = {0};                          // Minimum horizontal spacing between two columns. Preferably > (FramePadding.x + 1).
    style.ScrollbarSize                    = 10.0f;                        // Width of the vertical scrollbar, Height of the horizontal scrollbar.
    style.ScrollbarRounding                = RADIUS(0.5);                  // Radius of grab corners for scrollbar.
    style.GrabMinSize                      = SPACING(1.5);                 // Minimum width/height of a grab box for slider/scrollbar.
    style.GrabRounding                     = RADIUS(0.5);                  // Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs.
    style.LogSliderDeadzone                = {0};                          // The size in pixels of the dead-zone around zero on logarithmic sliders that cross zero.
    style.ImageBorderSize                  = 1.0f;                         // Thickness of border around Image() calls.
    style.TabRounding                      = {0};                          // Radius of upper corners of a tab. Set to 0.0f to have rectangular tabs.
    style.TabBorderSize                    = {0};                          // Thickness of border around tabs.
    style.TabCloseButtonMinWidthSelected   = {0};                          // -1: always visible. 0.0f: visible when hovered. >0.0f: visible when hovered if minimum width.
    style.TabCloseButtonMinWidthUnselected = {0};                          // -1: always visible. 0.0f: visible when hovered. >0.0f: visible when hovered if minimum width. FLT_MAX: never show close button when unselected.
    style.TabBarBorderSize                 = {0};                          // Thickness of tab-bar separator, which takes on the tab active color to denote focus.
    style.TabBarOverlineSize               = {0};                          // Thickness of tab-bar overline, which highlights the selected tab-bar.
    style.TableAngledHeadersAngle          = {0};                          // Angle of angled headers (supported values range from -50.0f degrees to +50.0f degrees).
    style.TableAngledHeadersTextAlign      = ImVec2();                     // Alignment of angled headers within the cell
    // style.ButtonTextAlign = ImVec2();            // Alignment of button text when button is larger than text. Defaults to (0.5f, 0.5f) (centered).
    style.SelectableTextAlign     = ImVec2(); // Alignment of selectable text. Defaults to (0.0f, 0.0f) (top-left aligned). It's generally important to keep this left-aligned if you want to lay multiple items on a same line.
    style.SeparatorTextBorderSize = {0};      // Thickness of border in SeparatorText()
    style.SeparatorTextAlign      = ImVec2(); // Alignment of text within the separator. Defaults to (0.0f, 0.5f) (left aligned, center).
    style.SeparatorTextPadding    = ImVec2(); // Horizontal offset of text from each edge of the separator + spacing on other axis. Generally small values. .y is recommended to be == FramePadding.y.
    style.DisplayWindowPadding    = ImVec2(); // Apply to regular windows: amount which we enforce to keep visible when moving near edges of your screen.
    style.DisplaySafeAreaPadding  = ImVec2(); // Apply to every windows, menus, popups, tooltips: amount where we avoid displaying contents. Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).
    style.MouseCursorScale        = {0};      // Scale software rendered mouse cursor (when io.MouseDrawCursor is enabled). We apply per-monitor DPI scaling over this scale. May be removed later.
    style.AntiAliasedLines        = 1;        // Enable anti-aliased lines/borders. Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to ImDrawList).
    style.AntiAliasedLinesUseTex  = 1;        // Enable anti-aliased lines/borders using textures where possible. Require backend to render with bilinear filtering (NOT point/nearest filtering). Latched at the beginning of the frame (copied to ImDrawList).
    style.AntiAliasedFill         = 1;        // Enable anti-aliased edges around filled shapes (rounded rectangles, circles, etc.). Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to ImDrawList).

    style.FontScaleDpi = 1.125;

    style.Colors[ImGuiCol_Text]                      = DARK_FOREGROUND;
    style.Colors[ImGuiCol_TextDisabled]              = DARK_MUTED_FOREGROUND;
    style.Colors[ImGuiCol_WindowBg]                  = DARK_BACKGROUND; // Background of normal windows
    style.Colors[ImGuiCol_ChildBg]                   = DARK_BACKGROUND; // Background of child windows
    style.Colors[ImGuiCol_PopupBg]                   = DARK_POPOVER;    // Background of popups, menus, tooltips windows
    style.Colors[ImGuiCol_Border]                    = DARK_BORDER;
    style.Colors[ImGuiCol_BorderShadow]              = ImVec4(1, 0, 0, 1);
    style.Colors[ImGuiCol_FrameBg]                   = DARK_SECONDARY; // Background of checkbox, radio button, plot, slider, text input
    style.Colors[ImGuiCol_FrameBgHovered]            = DARK_SECONDARY_HOVER;
    style.Colors[ImGuiCol_FrameBgActive]             = DARK_SECONDARY_HOVER;
    style.Colors[ImGuiCol_TitleBg]                   = DARK_BACKGROUND;       // Title bar
    style.Colors[ImGuiCol_TitleBgActive]             = DARK_BACKGROUND_HOVER; // Title bar when focused
    style.Colors[ImGuiCol_TitleBgCollapsed]          = DARK_BACKGROUND;       // Title bar when collapsed
    style.Colors[ImGuiCol_MenuBarBg]                 = DARK_BACKGROUND;
    style.Colors[ImGuiCol_ScrollbarBg]               = ImVec4();
    style.Colors[ImGuiCol_ScrollbarGrab]             = DARK_PRIMARY;
    style.Colors[ImGuiCol_ScrollbarGrabHovered]      = DARK_PRIMARY;
    style.Colors[ImGuiCol_ScrollbarGrabActive]       = DARK_PRIMARY;
    style.Colors[ImGuiCol_CheckMark]                 = DARK_PRIMARY_FOREGROUND; // Checkbox tick and RadioButton circle
    style.Colors[ImGuiCol_SliderGrab]                = DARK_PRIMARY_FOREGROUND;
    style.Colors[ImGuiCol_SliderGrabActive]          = DARK_PRIMARY;
    style.Colors[ImGuiCol_Button]                    = DARK_SECONDARY;
    style.Colors[ImGuiCol_ButtonHovered]             = DARK_PRIMARY_HOVER;
    style.Colors[ImGuiCol_ButtonActive]              = DARK_PRIMARY_HOVER;
    style.Colors[ImGuiCol_Header]                    = DARK_CARD; // Header* colors are used for CollapsingHeader, TreeNode, Selectable, MenuItem
    style.Colors[ImGuiCol_HeaderHovered]             = DARK_BACKGROUND_HOVER;
    style.Colors[ImGuiCol_HeaderActive]              = DARK_BACKGROUND_HOVER;
    style.Colors[ImGuiCol_Separator]                 = ImVec4(1, 0, 0, 1);
    style.Colors[ImGuiCol_SeparatorHovered]          = ImVec4(1, 0, 0, 1);
    style.Colors[ImGuiCol_SeparatorActive]           = ImVec4(1, 0, 0, 1);
    style.Colors[ImGuiCol_ResizeGrip]                = ImVec4(1, 0, 0, 1); // Resize grip in lower-right and lower-left corners of windows.
    style.Colors[ImGuiCol_ResizeGripHovered]         = ImVec4(1, 0, 0, 1);
    style.Colors[ImGuiCol_ResizeGripActive]          = ImVec4(1, 0, 0, 1);
    style.Colors[ImGuiCol_InputTextCursor]           = DARK_INPUT;         // InputText cursor/caret
    style.Colors[ImGuiCol_TabHovered]                = ImVec4(1, 0, 0, 1); // Tab background, when hovered
    style.Colors[ImGuiCol_Tab]                       = ImVec4(1, 0, 0, 1); // Tab background, when tab-bar is focused & tab is unselected
    style.Colors[ImGuiCol_TabSelected]               = ImVec4(1, 0, 0, 1); // Tab background, when tab-bar is focused & tab is selected
    style.Colors[ImGuiCol_TabSelectedOverline]       = ImVec4(1, 0, 0, 1); // Tab horizontal overline, when tab-bar is focused & tab is selected
    style.Colors[ImGuiCol_TabDimmed]                 = ImVec4(1, 0, 0, 1); // Tab background, when tab-bar is unfocused & tab is unselected
    style.Colors[ImGuiCol_TabDimmedSelected]         = ImVec4(1, 0, 0, 1); // Tab background, when tab-bar is unfocused & tab is selected
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(1, 0, 0, 1); //..horizontal overline, when tab-bar is unfocused & tab is selected
    style.Colors[ImGuiCol_PlotLines]                 = DARK_PRIMARY;
    style.Colors[ImGuiCol_PlotLinesHovered]          = DARK_PRIMARY_HOVER;
    style.Colors[ImGuiCol_PlotHistogram]             = ImVec4(1, 0, 0, 1);
    style.Colors[ImGuiCol_PlotHistogramHovered]      = ImVec4(1, 0, 0, 1);
    style.Colors[ImGuiCol_TableHeaderBg]             = ImVec4(1, 0, 0, 1); // Table header background
    style.Colors[ImGuiCol_TableBorderStrong]         = ImVec4(1, 0, 0, 1); // Table outer and header borders (prefer using Alpha=1.0 here)
    style.Colors[ImGuiCol_TableBorderLight]          = ImVec4(1, 0, 0, 1); // Table inner borders (prefer using Alpha=1.0 here)
    style.Colors[ImGuiCol_TableRowBg]                = ImVec4(1, 0, 0, 1); // Table row background (even rows)
    style.Colors[ImGuiCol_TableRowBgAlt]             = ImVec4(1, 0, 0, 1); // Table row background (odd rows)
    style.Colors[ImGuiCol_TextLink]                  = ImVec4(1, 0, 0, 1); // Hyperlink color
    style.Colors[ImGuiCol_TextSelectedBg]            = DARK_PRIMARY_HOVER; // Selected text inside an InputText
    style.Colors[ImGuiCol_TreeLines]                 = ImVec4(1, 0, 0, 1); // Tree node hierarchy outlines when using ImGuiTreeNodeFlags_DrawLines
    style.Colors[ImGuiCol_DragDropTarget]            = ImVec4(1, 0, 0, 1); // Rectangle border highlighting a drop target
    style.Colors[ImGuiCol_DragDropTargetBg]          = ImVec4(1, 0, 0, 1); // Rectangle background highlighting a drop target
    style.Colors[ImGuiCol_UnsavedMarker]             = ImVec4(1, 0, 0, 1); // Unsaved Document marker (in window title and tabs)
    style.Colors[ImGuiCol_NavCursor]                 = DARK_FOREGROUND;    // Color of keyboard/gamepad navigation cursor/rectangle, when visible
    style.Colors[ImGuiCol_NavWindowingHighlight]     = ImVec4(1, 0, 0, 1); // Highlight window when using Ctrl+Tab
    style.Colors[ImGuiCol_NavWindowingDimBg]         = ImVec4(1, 0, 0, 1); // Darken/colorize entire screen behind the Ctrl+Tab window list, when active
    style.Colors[ImGuiCol_ModalWindowDimBg]          = ImVec4(1, 0, 0, 1); // Darken/colorize entire screen behind a modal window, when one is active
}
