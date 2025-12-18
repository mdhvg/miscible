// ImGUI
#include "ui_core.h"
#include "base/array.h"
#define IMGUI_IMPLEMENTATION
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
// #define IMGUI_ENABLE_FREETYPE
#include "misc/single_file/imgui_single_file.h"

// .h
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "gl/gl_core.h"
#include "stb_image.h"
#include "ui/pages/menu.h"
#include "ui/pages/preview.h"

// .c, .cpp
#include "backends/imgui_impl_glfw.cpp"
#include "backends/imgui_impl_opengl3.cpp"
#include "gl/gl_core.cpp"

#include "atlas/atlas_render.h"
#include "IconsLucide.h"
#include "Window/Window.h"
#include "base/base_core.h"
#include "base/threadpool.h"
#include "db/db_helpers.h"
#include "os/os_inc.h"

// internal UIState state = {};
// internal PrevState prev;

struct AtlasScan
{
	U64 db_id;
	Path path;
};

local DynamicArray(AtlasScan, atlas_scan_array) = {0};
local Arena *atlas_scan_arena					= NULL;

void ui_init()
{
	arena_db_alloc(&ui_persist.ui_arena, KB(100));
	ui_persist.texture_data = dyn_array_init(pics.persistent_arena, 1 << 16, AtlasTexture);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.IniFilename = NULL;
	io.LogFilename = NULL;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	  // Enable Docking
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	  // Enable
	// Multi-Viewport

	io.ConfigViewportsNoTaskBarIcon		 = true;
	io.ConfigErrorRecoveryEnableTooltip	 = true;
	io.ConfigErrorRecovery				 = true;
	io.ConfigErrorRecoveryEnableDebugLog = true;

	// Setup Dear ImGui style
	// ImGui::StyleColorsDark();

	ImGuiStyle &style = ImGui::GetStyle();
	style			  = ImGuiStyle();

	// Reset ALL styles
	style.Alpha							   = 1;			   // Global alpha applies to everything in Dear ImGui.
	style.DisabledAlpha					   = {0};		   // Additional alpha multiplier applied by BeginDisabled(). Multiply over current value of Alpha.
	style.WindowPadding					   = ImVec2();	   // Padding within a window.
	style.WindowRounding				   = {0};		   // Radius of window corners rounding. Set to 0.0f to have rectangular windows. Large values tend to lead to variety of artifacts and are not recommended.
	style.WindowBorderSize				   = {0};		   // Thickness of border around windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
	style.WindowMinSize					   = ImVec2(1, 1); // Minimum window size. This is a global setting. If you want to constrain individual windows, use SetNextWindowSizeConstraints().
	style.WindowTitleAlign				   = ImVec2();	   // Alignment for title bar text. Defaults to (0.0f,0.5f) for left-aligned,vertically centered.
	style.ChildRounding					   = {0};		   // Radius of child window corners rounding. Set to 0.0f to have rectangular windows.
	style.ChildBorderSize				   = {0};		   // Thickness of border around child windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
	style.PopupRounding					   = {0};		   // Radius of popup window corners rounding. (Note that tooltip windows use WindowRounding)
	style.PopupBorderSize				   = {0};		   // Thickness of border around popup/tooltip windows. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
	style.FramePadding					   = ImVec2();	   // Padding within a framed rectangle (used by most widgets).
	style.FrameRounding					   = {0};		   // Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most widgets).
	style.FrameBorderSize				   = {0};		   // Thickness of border around frames. Generally set to 0.0f or 1.0f. (Other values are not well tested and more CPU/GPU costly).
	style.ItemSpacing					   = ImVec2();	   // Horizontal and vertical spacing between widgets/lines.
	style.ItemInnerSpacing				   = ImVec2();	   // Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label).
	style.CellPadding					   = ImVec2();	   // Padding within a table cell. Cellpadding.x is locked for entire table. CellPadding.y may be altered between different rows.
	style.TouchExtraPadding				   = ImVec2();	   // Expand reactive bounding box for touch-based system where touch position is not accurate enough. Unfortunately we don't sort widgets so priority on overlap will always be given to the first widget. So don't grow this too much!
	style.IndentSpacing					   = {0};		   // Horizontal indentation when e.g. entering a tree node. Generally == (FontSize + FramePadding.x*2).
	style.ColumnsMinSpacing				   = {0};		   // Minimum horizontal spacing between two columns. Preferably > (FramePadding.x + 1).
	style.ScrollbarSize					   = {0};		   // Width of the vertical scrollbar, Height of the horizontal scrollbar.
	style.ScrollbarRounding				   = {0};		   // Radius of grab corners for scrollbar.
	style.GrabMinSize					   = {0};		   // Minimum width/height of a grab box for slider/scrollbar.
	style.GrabRounding					   = {0};		   // Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs.
	style.LogSliderDeadzone				   = {0};		   // The size in pixels of the dead-zone around zero on logarithmic sliders that cross zero.
	style.ImageBorderSize				   = {0};		   // Thickness of border around Image() calls.
	style.TabRounding					   = {0};		   // Radius of upper corners of a tab. Set to 0.0f to have rectangular tabs.
	style.TabBorderSize					   = {0};		   // Thickness of border around tabs.
	style.TabCloseButtonMinWidthSelected   = {0};		   // -1: always visible. 0.0f: visible when hovered. >0.0f: visible when hovered if minimum width.
	style.TabCloseButtonMinWidthUnselected = {0};		   // -1: always visible. 0.0f: visible when hovered. >0.0f: visible when hovered if minimum width. FLT_MAX: never show close button when unselected.
	style.TabBarBorderSize				   = {0};		   // Thickness of tab-bar separator, which takes on the tab active color to denote focus.
	style.TabBarOverlineSize			   = {0};		   // Thickness of tab-bar overline, which highlights the selected tab-bar.
	style.TableAngledHeadersAngle		   = {0};		   // Angle of angled headers (supported values range from -50.0f degrees to +50.0f degrees).
	style.TableAngledHeadersTextAlign	   = ImVec2();	   // Alignment of angled headers within the cell
	style.ButtonTextAlign				   = ImVec2();	   // Alignment of button text when button is larger than text. Defaults to (0.5f, 0.5f) (centered).
	style.SelectableTextAlign			   = ImVec2();	   // Alignment of selectable text. Defaults to (0.0f, 0.0f) (top-left aligned). It's generally important to keep this left-aligned if you want to lay multiple items on a same line.
	style.SeparatorTextBorderSize		   = {0};		   // Thickness of border in SeparatorText()
	style.SeparatorTextAlign			   = ImVec2();	   // Alignment of text within the separator. Defaults to (0.0f, 0.5f) (left aligned, center).
	style.SeparatorTextPadding			   = ImVec2();	   // Horizontal offset of text from each edge of the separator + spacing on other axis. Generally small values. .y is recommended to be == FramePadding.y.
	style.DisplayWindowPadding			   = ImVec2();	   // Apply to regular windows: amount which we enforce to keep visible when moving near edges of your screen.
	style.DisplaySafeAreaPadding		   = ImVec2();	   // Apply to every windows, menus, popups, tooltips: amount where we avoid displaying contents. Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).
	style.DockingSeparatorSize			   = {0};		   // Thickness of resizing border between docked windows
	style.MouseCursorScale				   = {0};		   // Scale software rendered mouse cursor (when io.MouseDrawCursor is enabled). We apply per-monitor DPI scaling over this scale. May be removed later.
	style.AntiAliasedLines				   = {0};		   // Enable anti-aliased lines/borders. Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to ImDrawList).
	style.AntiAliasedLinesUseTex		   = {0};		   // Enable anti-aliased lines/borders using textures where possible. Require backend to render with bilinear filtering (NOT point/nearest filtering). Latched at the beginning of the frame (copied to ImDrawList).
	style.AntiAliasedFill				   = {0};		   // Enable anti-aliased edges around filled shapes (rounded rectangles, circles, etc.). Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame (copied to ImDrawList).

	// Colors
	// style.      Colors = {ImVec4(0,0,0,0)};

	// Behaviors
	// (It is possible to modify those fields mid-frame if specific behavior need it, unlike e.g. configuration fields in ImGuiIO)
	// float             HoverStationaryDelay;     // Delay for IsItemHovered(ImGuiHoveredFlags_Stationary). Time required to consider mouse stationary.
	// float             HoverDelayShort;          // Delay for IsItemHovered(ImGuiHoveredFlags_DelayShort). Usually used along with HoverStationaryDelay.
	// float             HoverDelayNormal;         // Delay for IsItemHovered(ImGuiHoveredFlags_DelayNormal). "
	// ImGuiHoveredFlags HoverFlagsForTooltipMouse;// Default flags when using IsItemHovered(ImGuiHoveredFlags_ForTooltip) or BeginItemTooltip()/SetItemTooltip() while using mouse.
	// ImGuiHoveredFlags HoverFlagsForTooltipNav;  // Default flags when using IsItemHovered(ImGuiHoveredFlags_ForTooltip) or BeginItemTooltip()/SetItemTooltip() while using keyboard/gamepad.

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(win.handle, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	ui_persist.ui_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist-VariableFont_wght.ttf", 18, NULL, io.Fonts->GetGlyphRangesDefault());

	static const ImWchar icons_ranges[] = {ICON_MIN_LC, ICON_MAX_16_LC, 0};
	// float icon_font_size = 18 * 2.0f / 3.0f;
	ImFontConfig icon_font_config;
	icon_font_config.MergeMode	= true;
	icon_font_config.PixelSnapH = true;
	// icon_font_config.GlyphMinAdvanceX = icon_font_size;
	ui_persist.icon_font  = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/lucide.ttf", 18, &icon_font_config, icons_ranges);
	ui_persist.title_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist-VariableFont_wght.ttf", 36, NULL, io.Fonts->GetGlyphRangesDefault());
	// PERF(SET_ORDER, set_order(state.sorting));
}

void ui_close()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void ui_update()
{
	arena_db_switch(&ui_persist.ui_arena);
	if (ui_persist.sidebar_open)
	{
		ui_persist.sidebar_width = 200;
	}
	else
	{
		ui_persist.sidebar_width = 48;
	}

	// if (state.active_image != prev.last_image)
	// {
	// 	// threadpool_enqueue({load_texture_2d, });
	// 	prev.last_image = state.active_image;
	// }

	// state.fps = 1 / delta;
	// if (state.sorting != prev.sorting)
	// {
	// 	PERF(SET_ORDER, set_order(state.sorting));
	// 	prev.sorting = state.sorting;
	// }

	// if (Application::get_instance().img_man.images.size() != image_count) {
	//   PERF(SET_ORDER, set_order(state.sorting));
	//   image_count = Application::get_instance().img_man.images.size();
	// }
}

// internal void set_order(SortMode selection)
// {
// 	const char *order_str;
// 	switch (selection)
// 	{
// 	case SortMode::SORT_FILENAME:
// 		order_str = "filename";
// 	default:
// 		order_str = "filename";
// 	}

// 	std::vector<unsigned int> temp;
// 	String statement = string_format(pics.ui_arena->arena, "SELECT id FROM Images ORDER BY %s;", order_str);
// 	db_run(
// 		(const char *)statement.value,
// 		[](void *data, int, char **argv, char **) {
// 			auto temp = (std::vector<unsigned int> *)data;
// 			temp->push_back(strtoul(argv[0], NULL, 0));
// 			return 0;
// 		},
// 		&temp);
// 	order.swap(temp);
// }

// inline const char *SortToString(SortMode s)
// {
// 	switch (s)
// 	{
// 	case SortMode::SORT_FILENAME:
// 		return "Filename";
// 	default:
// 		return "Unknown";
// 	}
// }

void ui_shutdown()
{
}

void ui_render()
{
	// if (state.view == MENU)
	// {
	// 	ui_menu();
	// }
	// else
	// {
	// 	ui_preview();
	// }
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ui_menu();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
}

DB_CALLBACK(set_atlas_data)
{
	AtlasScan v = {strtoull(argv[0], NULL, 0), s_cpy(atlas_scan_arena, argv[1])};
	dyn_array_push(atlas_scan_arena, atlas_scan_array, v);
	return 0;
}

THREAD_FUNC(atlas_load)
{
	AtlasScan v = dyn_array_at(atlas_scan_array, n);
	for (U64 i = 0; i < ui_persist.texture_data.size; i++)
	{
		if (dyn_array_at(ui_persist.texture_data, i).db_id == v.db_id)
		{
			printf("Skipped loading %.*s\n", v.path.size, v.path.v);
			return;
		}
	}

	S32 w, h;
	AtlasTexture d = {v.db_id, stbi_load(str_to_cstr(v.path), &w, &h, NULL, 3)};
	dyn_array_push(pics.persistent_arena, ui_persist.texture_data, d);
}

void ui_count_atlas()
{
	if (!atlas_scan_arena) atlas_scan_arena = arena_alloc(KB(100));
	arena_clear(atlas_scan_arena);

	U64 new_found = 0;
	db_run("SELECT COUNT(id) FROM Atlas;", get_count, &new_found);

	atlas_scan_array = dyn_array_init(atlas_scan_arena, new_found, AtlasScan);
	db_run("SELECT id, atlas_path FROM Atlas;", set_atlas_data);

	parallel_for(os_info.pool, atlas_scan_array.size, atlas_load, NULL, NULL);
}

void ui_after_load()
{
	for (U32 i = 0; i < ui_persist.texture_data.size; i++)
	{
		if (!dyn_array_at(ui_persist.texture_data, i).texture_id)
		{
			gl_make_texture(&dyn_array_at(ui_persist.texture_data, i).texture_id, dyn_array_at(ui_persist.texture_data, i).data, ATLAS_SIZE, ATLAS_SIZE, 3);
			dyn_array_at(ui_persist.texture_data, i).loaded = 1;
			stbi_image_free(dyn_array_at(ui_persist.texture_data, i).data);
		}
	}
}