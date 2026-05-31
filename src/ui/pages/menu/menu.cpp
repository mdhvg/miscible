#include "base/log.h"
#include "ui/pages/menu/menu.h"
#include "imgui.h"
#include "imgui_internal.h" // Required for ImGui::DockBuilder API
#include "db/view.h"
#include "miscible.h"
#include "ui/theme.h"
#include "scan/scan.h"
#include "ui/ui_core.h"
#include "base/array.h"
#include "config.h"
#include "base/string.h"
#include "db/fetch.h"
#include "base/base_core.h"
#include "IconsMaterialSymbols.h"

// Runtime state tracks
F32 sidebar_bottom_height = 0.0f;
F32 sidebar_width = 0.0f;
U32 sidebar_open = 0;
B32 needs_rebuild = 1;
S32 zoom_index = 0;
DirKey selected_directory = 0;

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
            ImGuiTreeNodeFlags_FramePadding |
            ImGuiTreeNodeFlags_DrawLinesFull,
        "");
    ImGui::SameLine();

    DirTree dir = dir_tree[cur];
    StringBuilder sb = string_empty(ui_state.page_arena);
    const char *dirname = format_cstr(&sb, ICON_MS_FOLDER " %.*s", StringSpr(dir.name));
    Assert(dirname, "dirname is null");

    ImGui::PushStyleColor(ImGuiCol_Button, (selected_directory == cur)
                                               ? (MSCBL_INTERACTION_HOVER)
                                               : (COLOR_TRANSPARENT));
    if (ImGui::Button(dirname, {ImGui::GetContentRegionAvail().x, 0}))
    {
        if (selected_directory == cur)
            selected_directory = 0;
        else
            selected_directory = cur;
    }
    ImGui::PopStyleColor();

    if (expand)
    {
        directory_tree(dir_tree[cur].child_idx);
        ImGui::TreePop();
    }

    directory_tree(dir_tree[cur].next_idx);
}

void sidebar_directories()
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Directories");

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ICON_MS_ADD).x);

    if (ImGui::Button(ICON_MS_ADD))
        scan_new_dir();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {MSCBL_INNER_PADDING, MSCBL_INNER_PADDING});
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0, 0});

    ImGui::BeginChild("Directories", ImVec2(0, ImGui::GetContentRegionAvail().y - sidebar_bottom_height - MSCBL_INNER_PADDING), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);

    ImGui::PushStyleColor(ImGuiCol_Border, COLOR_TRANSPARENT);
    directory_tree();
    ImGui::PopStyleColor();

    ImGui::EndChild();

    ImGui::PopStyleVar(2);
}

void draw_docked_sidebar(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));
    ImGui::Begin("SidebarPanel", NULL, flags);

    if (ImGui::Button(sidebar_open ? ICON_MS_CHEVRON_LEFT : ICON_MS_CHEVRON_RIGHT, {ImGui::GetContentRegionAvail().x, 0}))
    {
        if (sidebar_open)
        {
            sidebar_open = 0;
            sidebar_width = SPACING(sidebar_collapsed_units);
        }
        else
        {
            sidebar_open = 1;
            sidebar_width = SPACING(sidebar_open_units);
        }

        needs_rebuild = 1;
    }

    if (ImGui::Button(sidebar_open ? ICON_MS_CACHED "  Rescan Images" : ICON_MS_CACHED, {ImGui::GetContentRegionAvail().x, 0}))
    {
        // threadpool_enqueue({cont_scan});
    }

    if (sidebar_open)
    {
        ImGui::Separator();
        sidebar_directories();
    }
    else
    {
        ImGui::Dummy(ImVec2(0, 0));
    }

    float target_cursor_y = ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y - sidebar_bottom_height;
    ImGui::SetCursorPosY(target_cursor_y);
    ImGui::BeginChild("SidebarBottom", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AutoResizeY);
    if (ImGui::Button(sidebar_open ? ICON_MS_SETTINGS "  Settings" : ICON_MS_SETTINGS, {ImGui::GetContentRegionAvail().x, 0}))
    {
    }
    sidebar_bottom_height = ImGui::GetWindowHeight();
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
}

void main_grid()
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    F32 grid_spacing = SPACING(default_grid_spacing_units);

    S32 target_size = zoom_options[zoom_index].size;
    S32 cell_width = zoom_options[zoom_index].size + (S32)grid_spacing + 2;
    S32 cols = (avail.x > cell_width) ? ((S32)avail.x / cell_width) : 1;
    F32 req_width = (cell_width * cols) - (grid_spacing);
    F32 start = (avail.x - req_width) / 2.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, MSCBL_BACKGROUND);

    ImGui::BeginChild("Grid", {0, 0}, ImGuiChildFlags_None);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {grid_spacing, grid_spacing});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});

    StringBuilder sb = string_empty(ui_state.page_arena);
    S64 total_rendered = 0;

    ViewResult view_result = view_get_result();
    for (S64 grp = 0; grp < da_getsize(view_result.groups); grp++)
    {
        ViewResultGroup group = view_result.groups[grp];
        S64 group_limit = 0;
        switch (group.query_type)
        {
        case QueryType_Embedding:
            ImGui::Text("Semantic search");
            group_limit = max_semantic_results;
            break;
        case QueryType_FTS:
            ImGui::Text("Text search");
            break;
        default: break;
        }

        for (S64 off = 0;
             ((group_limit > 0) ? (off < group_limit && off < group.count) : (off < group.count));
             off++)
        {
            S64 id_idx = group.start_index + off;
            S64 image_id = view_result.image_ids[id_idx];

            ImGui::PushID((S32)image_id);

            if ((image_id < va_getsize(images)) &&
                (images[image_id].atlas_id < va_getsize(atlases)))
            {
                Image img = images[image_id];
                Atlas atl = atlases[img.atlas_id];

                F32 x = (F32)(img.atlas_idx % 10);
                F32 y = (F32)(img.atlas_idx / 10);

                ImGui::ImageButton(
                    "##ImgBtn",
                    atl.tex, {(F32)target_size, (F32)target_size}, {x / 10.0f, y / 10.0f}, {(x + 1) / 10.0f, (y + 1) / 10.0f});
            }
            else
            {
                ImGui::ImageButton(
                    "##ImgEmpty",
                    (ImTextureID)0, {(F32)target_size, (F32)target_size});
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, RADIUS(0.5));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {SPACING(1), SPACING(1)});
                ImGui::SetTooltip("%zu", image_id);
                ImGui::PopStyleVar(2);
            }

            total_rendered++;
            if (total_rendered % cols != 0)
                ImGui::SameLine();

            ImGui::PopID();
        }
        ImGui::NewLine();
    }

    ImGui::PopStyleVar(3);
    ImGui::EndChild();
    ImGui::PopStyleColor();
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

void zoom_controls()
{
    ImGui::SetNextItemWidth(SPACING(24.0f));
    if (ImGui::BeginCombo("##zoom", zoom_options[zoom_index].text, ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
    {
        for (S32 n = 0; n < StaticArrSize(zoom_options); n++)
        {
            B32 is_selected = (zoom_index == n);
            if (ImGui::Selectable(zoom_options[n].text, is_selected))
            {
                zoom_index = n;
            }
            if (is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void sort_controls()
{
    ImGui::SetNextItemWidth(SPACING(24.0f));
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
    case FilterType_SizeGreater: return "FILE SIZE";
    case FilterType_Path: return "PATH";
    case FilterType_DateModifiedAfter: return "DATE MODIFIED";
    case FilterType_DateCreatedAfter: return "DATE CREATED";
    case FilterType_EmbeddingDistanceGreater: return "MATCH %";
    default: Assert(0, "unknown filter added"); return 0;
    }
}

void vertical_bar(F32 width, F32 window_padding, ImVec4 color)
{
    ImVec2 start_pos = ImGui::GetCursorScreenPos();
    start_pos.y -= window_padding;
    ImVec2 end_pos = ImVec2(start_pos.x + width, start_pos.y + ImGui::GetFrameHeight() + window_padding * 2.0f);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(start_pos, end_pos, ImGui::ColorConvertFloat4ToU32(color));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + window_padding + width);
}

void filters()
{
    F32 max_chip_width = SPACING(56.0f);
    F32 total_width = ImGui::GetContentRegionAvail().x;

    UIFilter *filters = ui_state.view_query.filter_first;
    for (UIFilter *filter = &filters[0]; filter != NULL; filter = filter->next)
    {
        if (!filter->active)
            continue;

        ImGui::PushID((U64)filter);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::BeginChild((ImGuiID)(U64)filter, ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
        {

            // ImGui::PushStyleColor(ImGuiCol_FrameBg, COLOR_TRANSPARENT);
            vertical_bar(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING, filter->exclude ? MSCBL_STATE_SUBTRACTIVE : MSCBL_STATE_ADDITIVE);

            // ImGui::SetNextItemWidth(type_combo_w);
            if (ImGui::BeginCombo("##FilterType", filter_text(filter->type), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
            {
                for (U32 i = 0; i < FilterType_COUNT; i++)
                {
                    if (ImGui::Selectable(filter_text((FilterType)i), 1))
                    {
                        filter->type = (FilterType)i;
                        switch (filter->type)
                        {
                        case FilterType_SizeGreater:
                            filter->val_bytes = {0, Byte};
                            break;
                        case FilterType_Path:
                            filter->val_str = string_empty(ui_state.view_query.arena, 4096);
                            break;
                        case FilterType_DateCreatedAfter:
                        case FilterType_DateModifiedAfter:
                            filter->val_date = {22, Month_Apr, 2025};
                            break;
                        default:
                            break;
                        }
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, COLOR_TRANSPARENT);
            ImGui::SameLine();
            switch (filter->type)
            {
            case FilterType_Path:
                if (ImGui::SmallButton((filter->exclude) ? (ICON_MS_DO_NOT_DISTURB_ON) : (ICON_MS_ADD_CIRCLE)))
                    filter->exclude = !filter->exclude;
                break;
            case FilterType_SizeGreater:
            case FilterType_DateCreatedAfter:
            case FilterType_DateModifiedAfter:
            case FilterType_EmbeddingDistanceGreater:
                if (ImGui::SmallButton((filter->exclude) ? (ICON_MS_CHEVRON_BACKWARD) : (ICON_MS_CHEVRON_FORWARD)))
                    filter->exclude = !filter->exclude;
                break;
            default: break;
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();

            ImGui::SameLine();
            StringBuilder *buf_p = &filter->val_str;
            switch (filter->type)
            {
            case FilterType_Path:
                ImGui::SetNextItemWidth(SPACING(100.0f));
                ImGui::InputTextEx("##Input", "/path/to/file", CStrCast(filter->val_str), 512, ImVec2(0.0f, 0.0f), ImGuiInputTextFlags_CallbackEdit, text_callback, &buf_p);
                break;
            case FilterType_SizeGreater:
                ImGui::SetNextItemWidth(SPACING(16.0f));
                ImGui::DragFloat("##Bytes", &filter->val_float, 0.1f, 0.0f, 1023.0f, "%.2f");

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                ImGui::SameLine();
                if (ImGui::BeginCombo("##Multiplier", CStrCast(byte_string(filter->val_bytes.unit)), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_WidthFitPreview))
                {
                    for (U32 i = 0; i <= PiByte; i++)
                    {
                        if (ImGui::Selectable(CStrCast(byte_string((ByteUnit)i)), 1))
                            filter->val_bytes.unit = (ByteUnit)i;
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleVar();
                break;
            case FilterType_DateCreatedAfter:
            case FilterType_DateModifiedAfter:
                ImGui::SetNextItemWidth(SPACING(8.0f));
                ImGui::DragInt("##Date", (S32 *)&filter->val_date.date, 1.0f, 1, month_days(filter->val_date), "%02d", ImGuiSliderFlags_WrapAround);

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(SPACING(12.0f));
                if (ImGui::BeginCombo("##Month", CStrCast(month_string(filter->val_date.month)), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
                {
                    for (U32 i = 0; i <= Month_Dec; i++)
                    {
                        if (ImGui::Selectable(CStrCast(month_string((Month)i)), 1))
                            filter->val_date.month = (Month)i;
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                ImGui::SetNextItemWidth(SPACING(11.0f));
                ImGui::InputInt("##Year", &filter->val_date.year, 0, 0);
                ImGui::PopStyleVar();
                break;
            case FilterType_EmbeddingDistanceGreater:
                ImGui::SetNextItemWidth(SPACING(16.0f));
                ImGui::DragFloat("##Embedding", &filter->val_float, 1.0f, 0.0f, 100.0f, "%.2f%%");
                break;
            default: break;
            }

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 1.0f);

            ImGui::SameLine();
            if (ImGui::Button(ICON_MS_CLOSE))
            {
                ui_state.view_query.filter_empty++;
                filter->active = 0;
                UIFilter *fl = &filters[0];
                if (fl == ui_state.view_query.filter_first)
                    ui_state.view_query.filter_first = fl->next;
                else
                {
                    while (fl->next != filter)
                        fl = fl->next;
                    if (fl == ui_state.view_query.filter_last)
                        ui_state.view_query.filter_last = fl->next;

                    fl->next = filter->next;
                }
            }
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(2);

        ImGui::PopID();

        if (total_width - ImGui::GetCursorPosX() <= max_chip_width)
            ImGui::NewLine();
    }
}

void top_control_bar()
{
    F32 search_btn_width = ImGui::CalcTextSize("Search").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    F32 filter_btn_width = ImGui::CalcTextSize(ICON_MS_FILTER_LIST " Add Filter").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    F32 extra_widgets_w = SPACING(24.0f) + 160.0f + ImGui::GetStyle().ItemSpacing.x * 4.0f;
    F32 input_width = ImGui::GetContentRegionAvail().x - search_btn_width - extra_widgets_w - filter_btn_width;
    F32 minimum_boundary = SPACING(min_search_input_units);

    if (input_width < minimum_boundary)
        input_width = minimum_boundary;

    DeferLoop(ImGui::BeginGroup(), ImGui::EndGroup())
    {
        ImGui::SetNextItemWidth(input_width);
        StringBuilder *buf_p = &ui_state.view_query.search_query;
        B32 do_search = ImGui::InputTextWithHint("##Search", "Search...", CStrCast(ui_state.view_query.search_query), 4096, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit, text_callback, &buf_p);

        ImGui::SameLine();
        if (ImGui::Button("Search") || do_search)
        {
            view_clear_state();
            view_set_state(ui_state.view_query);
            view_reload();
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            view_clear_state();
            view_reload();
        }
    }

    DeferLoop(ImGui::BeginGroup(), ImGui::EndGroup())
    {
        ImGui::SameLine(0.0f, MSCBL_OUTER_PADDING);
        zoom_controls();

        ImGui::SameLine();
        sort_controls();

        ImGui::SameLine();
        if (ImGui::Button(ICON_MS_FILTER_LIST " Add Filter"))
        {
            UIFilter new_filter = {
                .active = 1,
                .next = 0,
                .type = FilterType_SizeGreater,
                .exclude = 0,
                .val_bytes = {
                    .value = 0,
                    .unit = Byte,
                }};

            S64 new_idx = 0;
            if (ui_state.view_query.filter_empty > 0)
            {
                ui_state.view_query.filter_empty--;
                for (S64 i = 0; i < da_getsize(ui_state.view_query.filter_first); i++)
                {
                    if (!ui_state.view_query.filter_first[i].active)
                    {
                        new_idx = i;
                        break;
                    }
                }
            }
            else
            {
                new_idx = da_getsize(ui_state.view_query.filter_first);
            }
            while (da_getsize(ui_state.view_query.filter_first) <= new_idx)
            {
                da_push(ui_state.view_query.arena, ui_state.view_query.filter_first, {0});
            }

            ui_state.view_query.filter_first[new_idx] = new_filter;
            if (ui_state.view_query.filter_last)
                ui_state.view_query.filter_last->next = &ui_state.view_query.filter_first[new_idx];
            ui_state.view_query.filter_last = &ui_state.view_query.filter_first[new_idx];
        }

        ImGui::SameLine();
        ImGui::Button(ICON_MS_SETTINGS); // TODO: Settings
    }

    filters();
}

void draw_docked_main(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));
    ImGui::Begin("MainPanel", NULL, flags);

    top_control_bar();

    ImGui::PopStyleVar();

    ImGui::ItemSize(ImVec2(0.0f, SPACING(1)));
    main_grid();

    ImGui::End();
}

void draw_docked_status(ImGuiWindowFlags flags)
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, MSCBL_SURFACE);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING));

    ImGui::Begin("StatusPanel", NULL, flags);

    ImGui::TextDisabled(ICON_MS_INFO " STATUS: ");
    ImGui::SameLine();
    ImGui::Text("READY");

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

MSCBL_EXP void ui_menu()
{
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGuiViewport *viewport = ImGui::GetMainViewport();

    // Safely check and populate fallback width configurations on startup loops
    if (sidebar_width == 0.0f)
    {
        sidebar_width = sidebar_open
                            ? SPACING(sidebar_open_units)
                            : SPACING(sidebar_collapsed_units);
    }

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_NoResize |
                                         ImGuiDockNodeFlags_NoSplit |
                                         ImGuiDockNodeFlags_NoTabBar |
                                         ImGuiDockNodeFlags_NoDockingOverMe |
                                         ImGuiDockNodeFlags_NoDockingSplit |
                                         ImGuiDockNodeFlags_NoDockingOverCentralNode |
                                         ImGuiDockNodeFlags_NoDockingOverEmpty |
                                         ImGuiDockNodeFlags_NoDocking |
                                         ImGuiDockNodeFlags_NoUndocking;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoDecoration;

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    if (ImGui::DockBuilderGetNode(dockspace_id) == NULL || needs_rebuild)
    {
        needs_rebuild = 0;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        ImGuiID dock_main_id = dockspace_id;

        F32 sidebar_ratio = sidebar_width / viewport->WorkSize.x;
        ImGuiID dock_id_sidebar = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, sidebar_ratio, NULL, &dock_main_id);

        F32 target_status_height = ImGui::GetFrameHeight();
        F32 status_bar_ratio = target_status_height / viewport->WorkSize.y;
        ImGuiID dock_id_status = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, status_bar_ratio, NULL, &dock_main_id);

        ImGui::DockBuilderDockWindow("SidebarPanel", dock_id_sidebar);
        ImGui::DockBuilderDockWindow("StatusPanel", dock_id_status);
        ImGui::DockBuilderDockWindow("MainPanel", dock_main_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    draw_docked_sidebar(window_flags);
    draw_docked_main(window_flags);
    draw_docked_status(window_flags);
}
