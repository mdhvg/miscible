// #define IMGUI_DISABLE_OBSOLETE_FUNCTIONS 1
#include "IconsMaterialSymbols.h"
#include "imgui.h"

#include "base/log.h"
#include "gl/gl_core.h"
#include "base/base_core.h"
#include "base/array.h"
#include "base/tree.h"
#include "ui/ui_core.h"
#include "ui/theme.h"

global_v const F32 sidebar_collapsed_w = SPACING(12);
global_v const F32 sidebar_open_w      = SPACING(80);
global_v const F32 statusbar_h         = REM(1) + SPACING(2);

struct MenuState
{
    F32 sidebar_width;
    U8 sidebar_open;
};

MenuState menu_state = {sidebar_collapsed_w};

extern "C" void ui_menu();
// {
//     SIDEBAR("Sidebar", menu_state.sidebar_width, win.height - menu_state.statusbar_height, ImGuiChildFlags_Borders,
//             BUTTON_GHOST(
//                 if (ImGui::Button(menu_state.sidebar_open
//                                       ? ICON_MS_CHEVRON_LEFT
//                                       : ICON_MS_CHEVRON_RIGHT,
//                                   {40, 36})) {
//                     menu_state.sidebar_open
//                         ? (menu_state.sidebar_open = 0, menu_state.sidebar_width = 48)
//                         : (menu_state.sidebar_open = 1, menu_state.sidebar_width = 320);
//                 }));
//
//     ImGui::SameLine();
//
//     ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1);
//     ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RADIUS(1) - 2);
//     ImGui::PushStyleColor(ImGuiCol_Border, DARK_BORDER);
//     ImGui::PushStyleColor(ImGuiCol_WindowBg, DARK_BACKGROUND);
//     ImGui::PushStyleColor(ImGuiCol_ChildBg, DARK_BACKGROUND);
//     ImGui::PushStyleColor(ImGuiCol_FrameBg, DARK_BACKGROUND);
//     ImGui::PushStyleColor(ImGuiCol_Button, DARK_BACKGROUND);
//     ImGui::SetCursorPos({menu_state.sidebar_width, 0});
//     ImGui::BeginChild("Main", {win.width - menu_state.sidebar_width, win.height - menu_state.statusbar_height}, ImGuiChildFlags_Borders);
//
//     GRID("Grid", win.width - menu_state.sidebar_width - 60, win.height - menu_state.statusbar_height - 100, ImGuiChildFlags_Borders,
//          {
//          // ImGui::InputText("Search", (char *)ui_persist.search_buffer, 2048);
//
//          // ImGui::PushFont(ui_persist.title_font);
//          // ImGui::Text(APP_NAME);
//          // ImGui::PopFont();
//
//          // ImGui::Text("FPS: %.2f", state.fps);
//
//          // if (ImGui::BeginCombo("##sort_order", "Sort ", ImGuiComboFlags_WidthFitPreview))
//          // {
//          // 	for (int n = 0; n < SORT_COUNT; n++)
//          // 	{
//          // 		SortMode val = (SortMode)n;
//          // 		bool is_selected = (state.sorting == val);
//          // 		if (ImGui::Selectable(SortToString(val), is_selected))
//          // 			state.sorting = val;
//          // 		if (is_selected)
//          // 			ImGui::SetItemDefaultFocus();
//          // 	}
//          // 	ImGui::EndCombo();
//          // }
//
//          // for (U32 i = 0; i < ui_persist.texture_data.size; i++)
//          // {
//          // 	ImGui::Image(dyn_array_at(ui_persist.texture_data, i).texture_id, ImVec2(ATLAS_SIZE / 4, ATLAS_SIZE / 4));
//          // }
//
//          float avail  = win.width - ImGui::GetCursorPosX();
//          float item_w = 256.0f; float spacing = 2 * (ImGui::GetStyle().ItemSpacing.x);
//
//          int cells = (int)((avail + spacing) / (item_w + spacing)); cells = MAX(cells, 1);
//
//          float used = cells * item_w + (cells - 1) * spacing;
//
//          float offset_x = (avail - used) * 0.5f; if (offset_x < 0.0f) offset_x = 0.0f;
//
//          // Apply the offset
//          ImGui::SameLine();
//
//          if (ui_state.display_order) {
//         U64 idx = 0;
//         while (idx < ui_state.image_count)
//         {
//             // Draw row
//             ImGui::Dummy({offset_x, 1});
//
//             for (int i = 0; i < cells && i + idx < ui_state.image_count; i++)
//             {
//                 Image *img = ui_state.display_order[idx + i];
//                 if (!img->atlas_tex)
//                 {
//                     Atlas_Node *atlas = tree_find(&ui_state.atlas, (Atlas *)&img->atlas_id, Atlas_cmp, Atlas);
//                     if (atlas && atlas->v.loaded)
//                         img->atlas_tex = atlas->v.tex;
//                 }
//                 U32 x = img->atlas_idx % 10;
//                 U32 y = img->atlas_idx / 10;
//
//                 ImGui::SameLine();
//                 if (ImGui::ImageButton("##",
//                                        img->atlas_tex,
//                                        {256, 256},
//                                        {(float)x / 10, (float)y / 10},
//                                        {(float)(x + 1) / 10, (float)(y + 1) / 10}))
//                 {
//                     // state.active_image = id;
//                     // active_index	   = i;
//                     // scroll_y		   = ImGui::GetScrollY();
//                     // state.view		   = PREVIEW;
//                 }
//             }
//             idx += cells;
//         } } });
//
//     ImGui::EndChild();
//     ImGui::PopStyleColor(5);
//     ImGui::PopStyleVar(2);
//
//     ImGui::SetCursorPos({0, win.height - menu_state.statusbar_height});
//     ImGui::BeginChild("StatusBar", {(F32)win.width, menu_state.statusbar_height});
//     ImGui::Text("Status");
//     ImGui::EndChild();
// }
