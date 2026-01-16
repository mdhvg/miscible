#include "base/base_core.h"
#include "base/string.h"
#include "imgui.h"
#include "ui/pages/menu.h"
#include "ui/components/button.h"
#include "ui/ui_core.h"

local_v const char *orders[] = {
    "Filename",
    "Date Added",
    "Date Modified",
    "Size",
};
local_v S32 selected = 0;

local_v const char *zooms[] = {
    "0.5x",
    "1x",
    "2x",
};
local_v F32 zoom_num[] = {
    0.5,
    1,
    2,
};
local_v S32 zoom_level   = 2;
local_v S32 base_size    = 128;
local_v F32 grid_spacing = SPACING(0.5);

local_v const char *dirs[] = {
    "Pinterest",
    "35mm",
    "Landscape",
    "2k25",
};
local_v S32 dir_len = 4;
local_v S32 dir_sel = 1;

void sidebar_directories()
{
    ImGui::BeginChild("Dirs", {ImGui::GetContentRegionAvail().x, 0});
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Directories");

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ICON_MS_ADD).x - SPACING(2));

    BUTTON_GHOST_START;
    ImGui::Button(ICON_MS_ADD);
    BUTTON_GHOST_END;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {SPACING(1), SPACING(1)});
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0, 0});
    for (S32 i = 0; i < dir_len; i++)
    {
        BUTTON_GHOST_START;
        if (i == dir_sel)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, DARK_ACCENT_HOVER);
            if (ImGui::Button(format_cstr(&strbuf, ICON_MS_FOLDER " %s", dirs[i]), {ImGui::GetContentRegionAvail().x, 0}))
                dir_sel = i;
            ImGui::PopStyleColor();
        }
        else
        {
            if (ImGui::Button(format_cstr(&strbuf, ICON_MS_FOLDER " %s", dirs[i]), {ImGui::GetContentRegionAvail().x, 0}))
                dir_sel = i;
        }
        BUTTON_GHOST_END;
    }
    ImGui::PopStyleVar(2);

    ImGui::EndChild();
}

void menu_sidebar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {SPACING(2), SPACING(2)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, SPACING(1)});
    ImGui::BeginChild("Sbar", {menu_state.sidebar_width, win.height}, ImGuiChildFlags_Borders);

    if (ImGui::Button(menu_state.sidebar_open
                          ? ICON_MS_CHEVRON_LEFT
                          : ICON_MS_CHEVRON_RIGHT))
    {
        menu_state.sidebar_open
            ? (menu_state.sidebar_open = 0, menu_state.sidebar_width = sidebar_collapsed_w)
            : (menu_state.sidebar_open = 1, menu_state.sidebar_width = sidebar_open_w);
    }

    if (ImGui::Button(menu_state.sidebar_open ? ICON_MS_CACHED "  Rescan Images" : ICON_MS_CACHED, {ImGui::GetContentRegionAvail().x, 0})) {}

    if (menu_state.sidebar_open)
        sidebar_directories();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
}

void main_grid()
{
    ImVec2 width   = ImGui::GetContentRegionAvail();
    S32 cell_width = base_size * zoom_num[zoom_level] + grid_spacing + 2;
    S32 cols       = (width.x - SPACING(12)) / cell_width;
    F32 req_width  = (cell_width * cols) - (grid_spacing);
    F32 start      = (width.x - req_width) / 2 + SPACING(6);

    ImGui::SetCursorPos({ImGui::GetCursorPosX() + start, ImGui::GetCursorPosY() + SPACING(12)});

    ImGui::PushItemWidth(req_width);
    ImGui::BeginChild("Grid", {req_width, 0});

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {grid_spacing, grid_spacing});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RADIUS(5));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});

    ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DARK_SECONDARY_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, DARK_SECONDARY_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_Border, DARK_BORDER);
    ImGui::PushStyleColor(ImGuiCol_BorderShadow, {0, 0, 0, 0});

    U32 t = 1;
    for (S32 i = 0; i < 400; i++)
    {
        if (ImGui::ImageButton(format_cstr(&strbuf, "##%d", i), (ImTextureRef)t, {base_size * zoom_num[zoom_level], base_size * zoom_num[zoom_level]}, {0, 0}, {1, 1}))
        {
            ui_state.active = i;
            ui_state.page   = UIPage_PREVIEW;
        }
        if ((i + 1) % cols)
            ImGui::SameLine();
    }

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(5);

    ImGui::EndChild();
    ImGui::PopItemWidth();
}

void menu_main()
{
    ImGui::BeginChild("Main", {0, win.height - statusbar_h});

    ImVec2 avail = ImGui::GetContentRegionAvail();

    if (ImGui::BeginCombo("##order", orders[selected], ImGuiComboFlags_WidthFitPreview))
    {
        for (S32 n = 0; n < 4; n++)
        {
            if (ImGui::Selectable(orders[n], selected == n))
            {
                selected = n;
            }
            if (selected == n)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Text(ICON_MS_ZOOM_OUT);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::PushItemWidth(150);
    ImGui::SliderInt("##zoom", &zoom_level, 0, 2, zooms[zoom_level]);
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
    ImGui::Text(ICON_MS_ZOOM_IN);

    main_grid();

    ImGui::EndChild();
}

void menu_status()
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + menu_state.sidebar_width);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + win.height - statusbar_h);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {SPACING(1), SPACING(1)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    ImGui::BeginChild("Stats", {win.width, statusbar_h}, ImGuiChildFlags_Borders);

    ImGui::Text("Stats");

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void ui_menu()
{
    // ImGui_child_params p = {0};
    // p.name               = "Sidebar";
    // p.width              = win.width - menu_state.sidebar_width;
    // p.height             = win.height - menu_state.statusbar_height;
    // // ImGui_child(p, {
    // // BUTTON_GHOST(
    // if (ImGui::Button(menu_state.sidebar_open
    //                       ? ICON_MS_CHEVRON_LEFT
    //                       : ICON_MS_CHEVRON_RIGHT))
    // {
    //     menu_state.sidebar_open
    //         ? (menu_state.sidebar_open = 0, menu_state.sidebar_width = 48)
    //         : (menu_state.sidebar_open = 1, menu_state.sidebar_width = 320);
    // }
    // // );
    // // });
    // // SIDEBAR("Sidebar", menu_state.sidebar_width, win.height - menu_state.statusbar_height, ImGuiChildFlags_Borders,
    // //
    // ImGui::SameLine();

    menu_sidebar();
    ImGui::SameLine();
    menu_main();
    menu_status();

    // MAIN("Main", win.width - menu_state.sidebar_width, win.height - menu_state.statusbar_height, ImGuiChildFlags_Borders,
    //      GRID("Grid", win.width - menu_state.sidebar_width - 60, win.height - menu_state.statusbar_height - 100, ImGuiChildFlags_Borders,
    //           {
    //     // ImGui::InputText("Search", (char *)ui_persist.search_buffer, 2048);
    //
    //     // ImGui::PushFont(ui_persist.title_font);
    //     // ImGui::Text(APP_NAME);
    //     // ImGui::PopFont();
    //
    //     // ImGui::Text("FPS: %.2f", state.fps);
    //
    //     // if (ImGui::BeginCombo("##sort_order", "Sort ", ImGuiComboFlags_WidthFitPreview))
    //     // {
    //     // 	for (int n = 0; n < SORT_COUNT; n++)
    //     // 	{
    //     // 		SortMode val = (SortMode)n;
    //     // 		bool is_selected = (state.sorting == val);
    //     // 		if (ImGui::Selectable(SortToString(val), is_selected))
    //     // 			state.sorting = val;
    //     // 		if (is_selected)
    //     // 			ImGui::SetItemDefaultFocus();
    //     // 	}
    //     // 	ImGui::EndCombo();
    //     // }
    //
    //     // for (U32 i = 0; i < ui_persist.texture_data.size; i++)
    //     // {
    //     // 	ImGui::Image(dyn_array_at(ui_persist.texture_data, i).texture_id, ImVec2(ATLAS_SIZE / 4, ATLAS_SIZE / 4));
    //     // }
    //
    //     float avail   = win.width - ImGui::GetCursorPosX();
    //     float item_w  = 256.0f;
    //     float spacing = 2 * (ImGui::GetStyle().ItemSpacing.x);
    //
    //     int cells = (int)((avail + spacing) / (item_w + spacing));
    //     cells     = MAX(cells, 1);
    //
    //     float used = cells * item_w + (cells - 1) * spacing;
    //
    //     float offset_x = (avail - used) * 0.5f;
    //     if (offset_x < 0.0f) offset_x = 0.0f;
    //
    //     // Apply the offset
    //     ImGui::SameLine();
    //
    //     if (ui_state.display_order)
    //     {
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
    //         }
    //     } }));
    //
    // ImGui::SetCursorPos({0, win.height - menu_state.statusbar_height});
    // ImGui::BeginChild("StatusBar", {(F32)win.width, menu_state.statusbar_height});
    // ImGui::Text("Status");
    // ImGui::EndChild();
}
