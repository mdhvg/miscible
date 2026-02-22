#include "os/win32/os_core_win32.h"
#include "scan.h"
#include "index.h"
#include "imgui.h"
#include "miscible.h"
#include "ui/theme.h"
#include "os/os_inc.h"
#include "base/tree.h"
#include "ui/ui_core.h"
#include "base/string.h"
#include "ui/ui_utils.h"
#include "base/base_core.h"
#include "base/threadpool.h"
#include "inference/model.h"
#include "IconsMaterialSymbols.h"
#include "ui/components/button.h"

#define sidebar_collapsed_w SPACING(12)
#define sidebar_open_w      SPACING(80)
#define statusbar_h         (REM(1) + SPACING(2))
#define main_w_ratio        0.9f

struct MenuState
{
    F32 sidebar_width;
    U32 sidebar_open;
};

MenuState menu_state = {sidebar_collapsed_w};

#define ZOOMS(X) \
    X(0.5)       \
    X(1)         \
    X(2)

#define X(a) #a,
local_v const char *zooms[] = {ZOOMS(X)};
#undef X

#define X(a) a,
local_v F32 zoom_num[] = {ZOOMS(X)};
#undef X

local_v S32 zoom_level   = 2;
local_v S32 base_size    = 128;
local_v F32 grid_spacing = SPACING(0.5);

local_v const char *dirs[] = {
    "Pinterest",
    "35mm",
    "Landscape",
    "2k25",
};
local_v S32 dir_len            = 4;
local_v S32 dir_sel            = 1;
local_v StringBuilder dir_path = {0};

void sidebar_directories()
{
    ImGui::BeginChild("Dirs", {ImGui::GetContentRegionAvail().x, 0});
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Directories");

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ICON_MS_ADD).x - SPACING(2));

    BUTTON_GHOST_START;
    if (ImGui::Button(ICON_MS_ADD))
    {
        if (!dir_path.size)
            dir_path = string_empty(ui_arena, 512);
        string_clear(dir_path);
        os_select_dir(W("Select Directory"), W(ROOT_DIR), &dir_path);
        if (dir_path.size)
        {
            put_dir(dir_path);
            scan_restart();
        }
    }
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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, DARK_SIDEBAR);
    ImGui::BeginChild("Sbar", {menu_state.sidebar_width, (F32)win.height}, ImGuiChildFlags_Borders);

    if (ImGui::Button(menu_state.sidebar_open
                          ? ICON_MS_CHEVRON_LEFT
                          : ICON_MS_CHEVRON_RIGHT))
    {
        menu_state.sidebar_open
            ? (menu_state.sidebar_open = 0, menu_state.sidebar_width = sidebar_collapsed_w)
            : (menu_state.sidebar_open = 1, menu_state.sidebar_width = sidebar_open_w);
    }

    if (ImGui::Button(menu_state.sidebar_open ? ICON_MS_CACHED "  Rescan Images" : ICON_MS_CACHED, {ImGui::GetContentRegionAvail().x, 0}))
    {
        async_job(os_info.pool, index_fill, NULL);
    }

    if (menu_state.sidebar_open)
        sidebar_directories();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void main_grid()
{
    ImVec2 avail   = ImGui::GetContentRegionAvail();
    S32 cell_width = base_size * zoom_num[zoom_level] + grid_spacing + 2;
    S32 cols       = avail.x / cell_width;
    F32 req_width  = (cell_width * cols) - (grid_spacing);
    F32 start      = (avail.x - req_width) / 2.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start);
    ImGui::BeginChild("Grid", {0, 0});

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {grid_spacing, grid_spacing});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RADIUS(10));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});

    ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DARK_SECONDARY_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, DARK_SECONDARY_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_Border, DARK_BORDER);
    ImGui::PushStyleColor(ImGuiCol_BorderShadow, {0, 0, 0, 0});

    if (ui_state.images)
    {
        for (S64 i = 0; i < da_getsize(ui_state.images); i++)
        {
            Image *img = ui_state.images + i;
            if (!img->atlas_tex)
            {
                Atlas_Node *atlas = tree_find(&ui_state.atlas, (Atlas *)&img->atlas_id, Atlas_cmp, Atlas);
                if (atlas && atlas->v.loaded)
                    img->atlas_tex = atlas->v.tex;
            }

            U32 x = img->atlas_idx % 10;
            U32 y = img->atlas_idx / 10;

            if (ImGui::ImageButton(format_cstr(&strbuf, "##%d", i), img->atlas_tex, {base_size * zoom_num[zoom_level], base_size * zoom_num[zoom_level]}, {(float)x / 10, (float)y / 10}, {(float)(x + 1) / 10, (float)(y + 1) / 10}))
            {
                ui_state.active = i;
                ui_state.page   = UIPage_PREVIEW;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
            {
                if (img->filename.size)
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, RADIUS(0.5));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {SPACING(1), SPACING(1)});
                    ImGui::PushStyleColor(ImGuiCol_PopupBg, DARK_BACKGROUND_HOVER);
                    ImGui::SetTooltip("%.*s", (S32)img->filename.size, img->filename.v);
                    ImGui::PopStyleVar();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                }
                else
                {
                    get_filename(img->id, &img->filename);
                }
            }

            if ((i + 1) % cols)
                ImGui::SameLine();
        }
    }

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(5);

    ImGui::EndChild();
    // ImGui::PopItemWidth();
}

S32 grow_buffer(ImGuiInputTextCallbackData *data)
{
    string_growto(&ui_state.search_buffer, data->BufTextLen);
    ui_state.search_buffer.size = data->BufTextLen;
    return 1;
}

void search()
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 pos   = ImGui::GetCursorPos();
    F32 child_w  = avail.x * main_w_ratio;
    F32 start    = (avail.x - child_w) / 2.0f;

    ImGui::SetCursorPos({pos.x + SPACING(2), pos.y + SPACING(2)});

    ImGui::InputTextWithHint("##Search", "Search term", (char *)ui_state.search_buffer.v, ui_state.search_buffer.capacity, ImGuiInputTextFlags_CallbackResize, grow_buffer);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SPACING(1));
    if (ImGui::Button("Search"))
        async_job(os_info.pool, embed_text, &ui_state.search_buffer);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SPACING(2));
}

void sort_controls()
{
    if (ImGui::BeginCombo("##order", "Sort by", ImGuiComboFlags_WidthFitPreview))
    {
        for (S32 n = 0; n < OrderBy_COUNT; n++)
        {
            if (ImGui::Selectable(order_str[n], current_sort.order_by == n))
            {
                current_sort.order_by = (OrderBy)n;
                refresh_results();
            }
            if (current_sort.order_by == n)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void zoom_controls()
{
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SPACING(0.8));
    ImGui::BeginChild("Zoom_Controls", {0, REM(2)});

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {SPACING(1), 0});
    ImGui::AlignTextToFramePadding();
    ImGui::Text(ICON_MS_ZOOM_OUT);
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::PushItemWidth(200);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SPACING(1));
    ImGui::SliderInt("##zoom", &zoom_level, 0, 2, zooms[zoom_level]);
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text(ICON_MS_ZOOM_IN);
    ImGui::PopStyleVar();

    ImGui::EndChild();
}

void controls()
{
    ImVec2 pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos({pos.x + SPACING(6), pos.y + SPACING(2)});
    ImGui::BeginChild("Controls", {0, REM(3)});

    sort_controls();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SPACING(2));
    zoom_controls();

    ImGui::EndChild();
}

void menu_main()
{
    ImGui::BeginChild("Main", {0, (F32)win.height - statusbar_h});

    search();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    F32 child_w  = avail.x * main_w_ratio;
    F32 start    = (avail.x - child_w) / 2.0f;

    ImVec2 pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos({pos.x + start, pos.y});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, DARK_CARD);
    ImGui::BeginChild("Container", {child_w, 0});

    controls();
    main_grid();

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::EndChild();
}

void menu_status()
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + menu_state.sidebar_width);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (F32)win.height - statusbar_h);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {SPACING(1), SPACING(1)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    ImGui::BeginChild("Stats", {(F32)win.width, statusbar_h}, ImGuiChildFlags_Borders);

    ImGui::Text("Stats");

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

MSCBL_EXP void ui_menu()
{
    menu_sidebar();
    ImGui::SameLine();
    menu_main();
    menu_status();
}
