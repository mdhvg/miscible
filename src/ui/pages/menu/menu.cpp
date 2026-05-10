#include "ui/pages/menu/menu.h"
#include "imgui.h"
#include "db/view.h"
#include "miscible.h"
#include "ui/pages/menu/menu.h"
#include "ui/widgets.h"
#include "ui/theme.h"
#include "scan/scan.h"
#include "ui/ui_core.h"
#include "base/array.h"
#include "base/arena.h"
#include "base/string.h"
#include "db/fetch.h"
#include "window/window.h"
#include "base/base_core.h"
#include "IconsMaterialSymbols.h"
#include "ui/components/button.h"

#define sidebar_collapsed_w SPACING(12)
#define sidebar_open_w      SPACING(80)
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

local_v S32 zoom_level = 2;
local_v S32 base_size = 128;
local_v F32 grid_spacing = SPACING(0.5);

// void new_filter(FilterList *list, FilterType type)
// {
//     ViewFilter *filter = push_struct(ui_state.arena, ViewFilter);
//     filter->type       = FilterType_SizeGreater;
//
//     if (list->last)
//     {
//         list->last->next = filter;
//     }
//     else
//     {
//         ViewFilter **cur = &list->head;
//         *cur             = filter;
//     }
//     list->last = filter;
// }

local_v DirKey selected = 0;

void directory_tree(DirKey cur = 1)
{
    if (da_getsize(dir_tree) <= 1)
        return;

    if (!cur)
        return;

    B32 expand = ImGui::TreeNodeEx(
        (void *)cur,
        ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            // ImGuiTreeNodeFlags_SpanFullWidth |
            // ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_FramePadding |
            ImGuiTreeNodeFlags_DrawLinesFull,
        "");
    ImGui::SameLine();

    // BUTTON_GHOST_START;
    if (cur == selected)
        ImGui::PushStyleColor(ImGuiCol_Button, DARK_ACCENT_HOVER);
    else
        ImGui::PushStyleColor(ImGuiCol_Button, DARK_ACCENT);

    DirTree dir = dir_tree[cur];
    StringBuilder sb = string_empty(ui_state.page_arena);
    const char *dirname = format_cstr(&sb, ICON_MS_FOLDER " %.*s", StringSpr(dir.name));
    Assert(dirname, "dirname is null");

    if (ImGui::Button(dirname, {ImGui::GetContentRegionAvail().x, 0}))
    {
        if (selected == cur)
            selected = 0;
        else
            selected = cur;
    }
    ImGui::PopStyleColor();
    // BUTTON_GHOST_END;

    if (expand)
    {
        directory_tree(dir_tree[cur].child_idx);
        ImGui::TreePop();
    }

    directory_tree(dir_tree[cur].next_idx);
}

void sidebar_directories()
{
    ImGui::BeginChild("Dirs", {ImGui::GetContentRegionAvail().x, 0}, ImGuiChildFlags_Borders);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Directories");

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ICON_MS_ADD).x - SPACING(2));

    BUTTON_GHOST_START;
    if (ImGui::Button(ICON_MS_ADD))
        scan_new_dir();
    BUTTON_GHOST_END;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {SPACING(1), SPACING(1)});
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0, 0});

    ImGui::BeginChild("Directories", {0, 0}, ImGuiChildFlags_Borders);
    directory_tree();
    ImGui::EndChild();

    // for (S32 i = 0; i < dir_len; i++)
    // {
    //     BUTTON_GHOST_START;
    //     if (i == dir_sel)
    //     {
    //         ImGui::PushStyleColor(ImGuiCol_Button, DARK_ACCENT_HOVER);
    //         if (ImGui::Button(format_cstr(&strbuf, ICON_MS_FOLDER " %s", dirs[i]), {ImGui::GetContentRegionAvail().x, 0}))
    //             dir_sel = i;
    //         ImGui::PopStyleColor();
    //     }
    //     else
    //     {
    //         if (ImGui::Button(format_cstr(&strbuf, ICON_MS_FOLDER " %s", dirs[i]), {ImGui::GetContentRegionAvail().x, 0}))
    //             dir_sel = i;
    //     }
    //     BUTTON_GHOST_END;
    // }
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

    if (ImGui::Button(menu_state.sidebar_open ? ICON_MS_CACHED "  Rescan Images" : ICON_MS_CACHED))
    {
        // threadpool_enqueue({cont_scan});
        // threadpool_enqueue(os_info.pool, {index_fill});
        // async_job(os_info.pool, index_fill, NULL);
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
    ImVec2 avail = ImGui::GetContentRegionAvail();
    S32 cell_width = base_size * zoom_num[zoom_level] + grid_spacing + 2;
    S32 cols = avail.x / cell_width;
    F32 req_width = (cell_width * cols) - (grid_spacing);
    F32 start = (avail.x - req_width) / 2.0f;

    // ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start);
    ImGui::BeginChild("Grid", {0, 0}, ImGuiChildFlags_Borders);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {grid_spacing, grid_spacing});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    // ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, RADIUS(10));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});

    ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DARK_SECONDARY_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, DARK_SECONDARY_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_Border, DARK_BORDER);
    ImGui::PushStyleColor(ImGuiCol_BorderShadow, {0, 0, 0, 0});

    StringBuilder sb = string_empty(ui_state.page_arena);
    U32 width_avail = ImGui::GetContentRegionAvail().x;
    U32 row_size = width_avail / 128;

    ViewResult view_result = view_get_result();
    for (S64 i = 0; i < da_getsize(view_result.groups); i++)
    {
        ViewResultGroup group = view_result.groups[i];
        switch (group.source)
        {
        case Source_Embedding:
            ImGui::Text("Semantic search");
            break;
        case Source_FTS:
            ImGui::Text("Text search");
            break;
        default: break;
        }

        for (S64 i = group.start_index; i < group.start_index + group.count; i++)
        {
            S64 image_id = view_result.image_ids[i];
            if (image_id < va_getsize(images) && images[image_id].atlas_id < va_getsize(atlases))
            {
                Image img = images[image_id];
                Atlas atl = atlases[img.atlas_id];

                F32 x = (F32)(img.atlas_idx % 10);
                F32 y = (F32)(img.atlas_idx / 10);

                ImGui::ImageButton(
                    format_cstr(&sb, "##%d", i),
                    atl.tex, {128, 128}, {x / 10.0f, y / 10.0f}, {(x + 1) / 10.0f, (y + 1) / 10.0f});
            }
            else
            {
                ImGui::ImageButton(
                    format_cstr(&sb, "##%d", i),
                    (ImTextureID)0, {128, 128});
            }
            if ((i + 1) % row_size)
                ImGui::SameLine();
        }
        ImGui::NewLine();
    }

    // if (ui_state.images)
    // {
    // for (S64 i = 0; i < da_getsize(ui_state.images); i++)
    // {
    //     Image *img = ui_state.images + i;
    //     if (!img->atlas_tex)
    //     {
    //         Atlas_Node *atlas = tree_find(&ui_state.atlas, (Atlas *)&img->atlas_id, Atlas_cmp, Atlas);
    //         if (atlas && atlas->v.loaded)
    //             img->atlas_tex = atlas->v.tex;
    //     }
    //
    //     U32 x = img->atlas_idx % 10;
    //     U32 y = img->atlas_idx / 10;
    //
    //     if (ImGui::ImageButton(format_cstr(&strbuf, "##%d", i), img->atlas_tex, {base_size * zoom_num[zoom_level], base_size * zoom_num[zoom_level]}, {(float)x / 10, (float)y / 10}, {(float)(x + 1) / 10, (float)(y + 1) / 10}))
    //     {
    //         ui_state.active = i;
    //         ui_state.page   = UIPage_PREVIEW;
    //     }
    //     if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
    //     {
    //         if (img->filename.size)
    //         {
    //             ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, RADIUS(0.5));
    //             ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {SPACING(1), SPACING(1)});
    //             ImGui::PushStyleColor(ImGuiCol_PopupBg, DARK_BACKGROUND_HOVER);
    //             ImGui::SetTooltip("%.*s", (S32)img->filename.size, img->filename.v);
    //             ImGui::PopStyleVar();
    //             ImGui::PopStyleVar();
    //             ImGui::PopStyleColor();
    //         }
    //         else
    //         {
    //             get_filename(img->id, &img->filename);
    //         }
    //     }
    //
    //     if ((i + 1) % cols)
    //         ImGui::SameLine();
    // }
    // }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);

    ImGui::EndChild();
    // ImGui::PopItemWidth();
}

S32 text_callback(ImGuiInputTextCallbackData *data)
{
    StringBuilder **buf = (StringBuilder **)data->UserData;

    if (data->BufTextLen >= (*buf)->size)
    {
        String input = {
            .v = (U8 *)(data->Buf + (*buf)->size),
            .size = (U64)(data->BufTextLen - (*buf)->size)};
        string_push(*buf, input);
    }
    else
    {
        string_pop_to((*buf), data->BufTextLen);
    }

    return 0;
}

void search()
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 pos = ImGui::GetCursorPos();
    F32 child_w = avail.x * main_w_ratio;
    B32 do_search = 0;

    // ImGui::SetCursorPos({pos.x + SPACING(2), pos.y + SPACING(2)});

    StringBuilder *buf_p = &ui_state.view_query.search_query;
    do_search |= (ImGui::InputTextWithHint("##Search", "Search term", CStrCast(ui_state.view_query.search_query), 4096, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit, text_callback, &buf_p) ? 1 : 0);

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SPACING(1));
    do_search |= (ImGui::Button("Search") ? 1 : 0);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SPACING(2));

    if (do_search)
    {
        view_clear_state();
        view_set_state(ui_state.view_query);
        view_reload();
    }
}

void sort_controls()
{
    if (ImGui::BeginCombo("##order", "Sort by", ImGuiComboFlags_WidthFitPreview))
    {
        for (S32 n = 0; n < StaticArrSize(sort_options); n++)
        {
            if (ImGui::Selectable(sort_options[n].text, 1))
                ui_state.view_query.sort_basis = (SortType)n;
            if (sort_options[n].kind == ui_state.view_query.sort_basis)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

global_v const char *filter_text(FilterType type)
{
    switch (type)
    {
    case FilterType_SizeGreater:
        return "FILE SIZE";
    case FilterType_Path:
        return "PATH";
    case FilterType_Filename:
        return "FILENAME";
    case FilterType_DateModifiedAfter:
        return "DATE MODIFIED";
    case FilterType_DateCreatedAfter:
        return "DATE CREATED";
    case FilterType_EmbeddingDistanceGreater:
        return "DISTANCE";
    default:
        Assert(0, "unknown filter added");
        return 0;
    }
}

void filter_accent()
{
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Draw a 4px wide bar. We offset it slightly left or use Dummy
    // to ensure it overlaps the border nicely.
    draw_list->AddRectFilled(p, ImVec2(p.x + 4.0f, p.y + 32.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.2f, 0.3f, 1.0f)));

    // Advance cursor so widgets don't draw over our bar
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f); // Move past the accent bar
}

void filters()
{
    // if (!filter_list.head)
    // {
    //     ViewFilter *head   = push_struct(ui_state.arena, ViewFilter);
    //     ViewFilter *filter = head;
    //     filter->type       = FilterType_Path;
    //     // StringCast(string_empty(ui_state.arena, 4096));
    //     filter->val_str = string_cpy(ui_state.arena, "* a path *");
    //
    //     filter->next    = push_struct(ui_state.arena, ViewFilter);
    //     filter          = filter->next;
    //     filter->type    = FilterType_Filename;
    //     filter->val_str = string_cpy(ui_state.arena, "* a filename *");
    //
    //     filter->next      = push_struct(ui_state.arena, ViewFilter);
    //     filter            = filter->next;
    //     filter->type      = FilterType_SizeGreater;
    //     filter->val_bytes = {104.53, KiByte};
    //
    //     filter->next = push_struct(ui_state.arena, ViewFilter);
    //     filter       = filter->next;
    //     filter->type = FilterType_DateCreatedAfter;
    //     // filter->val_int = 200;
    //
    //     filter->next = push_struct(ui_state.arena, ViewFilter);
    //     filter       = filter->next;
    //     filter->type = FilterType_DateModifiedAfter;
    //     // filter->val_int = 300;
    //
    //     filter_list.head = head;
    // }

    if (ImGui::Button(ICON_MS_FILTER_LIST))
    {
        U64 f1 = 0;
        for (U32 i = 1; i < da_getsize(ui_state.view_query.filters); i++)
        {
            if (!ui_state.view_query.filters[i].active)
            {
                f1 = i;
                break;
            }
        }
        if (f1 == 0)
        {
            da_push(ui_state.view_query.arena, ui_state.view_query.filters, {0});
            f1 = da_getsize(ui_state.view_query.filters) - 1;
        }
        ui_state.view_query.filters[f1] = {.active = 1, .next = 0, .type = FilterType_SizeGreater};
        ui_state.view_query.filters[ui_state.view_query.last].next = f1;
        ui_state.view_query.last = f1;
    }

    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    for (S64 fl = 1; fl != 0; fl = ui_state.view_query.filters[fl].next)
    {
        // ImGui::SameLine(0, 10.0f);
        UIFilter *filters = ui_state.view_query.filters;
        if (!filters[fl].active)
            continue;

        ImGui::PushID(fl);
        ImGui::BeginGroup();

        filter_accent();

        if (ImGui::BeginCombo("##FilterType", filter_text(filters[fl].type), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
        {
            for (U32 i = 0; i < FilterType_COUNT; i++)
            {
                if (ImGui::Selectable(filter_text((FilterType)i), 1))
                {
                    filters[fl].type = (FilterType)i;
                    switch (filters[fl].type)
                    {
                    case FilterType_SizeGreater:
                        filters[fl].val_bytes = {0, Byte};
                        break;
                    case FilterType_Path:
                    case FilterType_Filename:
                        filters[fl].val_str = string_empty(ui_state.view_query.arena, 4096);
                        break;
                    case FilterType_DateCreatedAfter:
                    case FilterType_DateModifiedAfter:
                        filters[fl].val_date = {22, Apr, 2025};
                        break;
                    default: break;
                    }
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        ImGui::Checkbox("##Exclude", (bool *)&filters[fl].exclude);

        ImGui::SameLine();

        StringBuilder *buf_p = &filters[fl].val_str;
        switch (filters[fl].type)
        {
        case FilterType_Path:
        case FilterType_Filename:
            ImGui::InputTextMultiline("##Input", CStrCast(filters[fl].val_str), KB(4), {320, 30}, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CtrlEnterForNewLine, text_callback, &buf_p);
            break;
        case FilterType_SizeGreater:
            input_bytesize(&filters[fl].val_bytes);
            break;
        case FilterType_DateCreatedAfter:
        case FilterType_DateModifiedAfter:
            input_date(&filters[fl].val_date);
            break;
            // case FilterType_EmbeddingDistanceGreater:
        default:
            break;
        }

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();
        if (ImGui::Button(ICON_MS_CLOSE))
            filters[fl].active = 0;

        ImGui::EndGroup();
        ImGui::PopID();
    }
}

void zoom_controls()
{
    // ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SPACING(0.8));
    // ImGui::BeginChild("Zoom_Controls", {0, REM(2)}, ImGuiChildFlags_Borders);

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

    // ImGui::EndChild();
}

void controls()
{
    // ImVec2 pos = ImGui::GetCursorPos();
    // ImGui::SetCursorPos({pos.x + SPACING(6), pos.y + SPACING(2)});
    // ImGui::BeginChild("Controls", {0,0}, ImGuiChildFlags_Borders);

    sort_controls();
    // ImGui::SameLine();
    // ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SPACING(2));
    zoom_controls();

    filters();

    // ImGui::EndChild();
}

void menu_main()
{
    ImGui::BeginChild("Main", {0, 0}, ImGuiChildFlags_Borders);

    search();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    F32 child_w = avail.x * main_w_ratio;
    F32 start = (avail.x - child_w) / 2.0f;

    // ImVec2 pos = ImGui::GetCursorPos();
    // ImGui::SetCursorPos({pos.x + start, pos.y});
    // ImGui::PushStyleColor(ImGuiCol_ChildBg, DARK_CARD);
    // ImGui::BeginChild("Container", {child_w, 0}, ImGuiChildFlags_Borders);

    controls();
    main_grid();

    // ImGui::EndChild();
    // ImGui::PopStyleColor();

    ImGui::EndChild();
}

// void menu_status()
// {
//     ImGui::SetCursorPosX(ImGui::GetCursorPosX() + menu_state.sidebar_width);
//     ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (F32)win.height - statusbar_h);
//     ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {SPACING(1), SPACING(1)});
//     ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
//     ImGui::BeginChild("Stats", {(F32)win.width, statusbar_h}, ImGuiChildFlags_Borders);
//
//     ImGui::Text("Stats");
//
//     ImGui::PopStyleVar();
//     ImGui::PopStyleVar();
//     ImGui::EndChild();
// }

MSCBL_EXP void ui_menu()
{
    menu_sidebar();
    ImGui::SameLine();
    menu_main();
    // menu_status();
}
