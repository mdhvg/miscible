#include "gl/gl_core.h"
#include "ui/pages/pages.h"
#include "ui/pages/menu/menu.h"

#include "config.h"
#include "db/view.h"
#include "db/fetch.h"
#include "base/log.h"
#include "ui/theme.h"
#include "scan/scan.h"
#include "base/array.h"
#include "ui/ui_core.h"

// Runtime state track
local_v U32 sidebar_open = 0;
local_v B32 sidebar_dirtree_openall = 0;
local_v B32 sidebar_dirtree_foldall = 0;

local_v S32 zoom_index = 1;
local_v DirKey selected_directory = 0;

void directory_tree(DirKey cur = 1)
{
    if (da_getsize(dir_tree) <= 1)
        return;

    if (!cur)
        return;

    if (sidebar_dirtree_openall)
        ImGui::SetNextItemOpen(1);
    if (sidebar_dirtree_foldall)
        ImGui::SetNextItemOpen(0);

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
    const char *dirname = format_cstr(&sb, ICON_LC_FOLDER " %.*s", StringSpr(dir.name));
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

void menu_draw_docked_sidebar(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));
    ImGui::Begin("SidebarPanel", NULL, flags);

    if (ImGui::Button(sidebar_open ? ICON_LC_CHEVRON_LEFT : ICON_LC_CHEVRON_RIGHT, {ImGui::GetContentRegionAvail().x, 0}))
    {
        sidebar_open = !sidebar_open;
        needs_rebuild = 1;
    }

    if (ImGui::Button(sidebar_open ? ICON_LC_REFRESH_CW "  Rescan Images" : ICON_LC_REFRESH_CW, {ImGui::GetContentRegionAvail().x, 0}))
    {
        // threadpool_enqueue(TaskPriority_High, {.func = cont_scan});
        ui_push_message({.success = 0, .domain = Domain_App, .code = 1, .context = "implementation pending"});
    }

    ImGuiStyle &style = ImGui::GetStyle();
    F32 frame_height = ImGui::GetFrameHeight();
    F32 sidebar_bottom_height = frame_height;
    if (sidebar_open)
    {
        sidebar_bottom_height += frame_height + style.ItemSpacing.y;
    }

    if (sidebar_open)
    {
        ImGui::Separator();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Directories");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ICON_LC_PLUS).x);
        if (ImGui::Button(ICON_LC_PLUS))
        {
            scan_new_dir();
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        F32 sidebar_middle_height = avail.y - sidebar_bottom_height - style.ItemSpacing.y;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {MSCBL_INNER_PADDING, MSCBL_INNER_PADDING});
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0, 0});
        DeferLoop(ImGui::BeginChild("Directories", ImVec2(avail.x, sidebar_middle_height), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY), ImGui::EndChild())
        {
            ImGui::PushStyleColor(ImGuiCol_Border, COLOR_TRANSPARENT);
            directory_tree();
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleVar(2);

        sidebar_dirtree_openall = 0;
        sidebar_dirtree_foldall = 0;
    }
    else
    {
        F32 sidebar_middle_height = ImGui::GetContentRegionAvail().y - sidebar_bottom_height - style.ItemSpacing.y;
        ImGui::Dummy(ImVec2(0.0f, sidebar_middle_height));
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_TRANSPARENT);
    DeferLoop(ImGui::BeginChild("SidebarBottom", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_AutoResizeY), ImGui::EndChild())
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGuiStyle &style = ImGui::GetStyle();

        if (sidebar_open)
        {
            if (ImGui::Button("Expand All", ImVec2((avail.x - style.ItemSpacing.x) / 2.0f, 0)))
                sidebar_dirtree_openall = 1;
            ImGui::SameLine();
            if (ImGui::Button("Collapse All", ImVec2((avail.x - style.ItemSpacing.x) / 2.0f, 0)))
                sidebar_dirtree_foldall = 1;
        }

        if (ImGui::Button(sidebar_open ? ICON_LC_SETTINGS "  Settings" : ICON_LC_SETTINGS, {avail.x, 0}))
        {
            ui_push_message({.success = 0, .domain = Domain_App, .code = 1, .context = "implementation pending"});
            // TODO: this
        }
        sidebar_bottom_height = ImGui::GetWindowHeight();
    }
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void main_grid()
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGuiStyle &style = ImGui::GetStyle();

    F32 thumbnail_size = zoom_options[zoom_index].size;
    S32 cell_width = thumbnail_size + style.ItemSpacing.x;
    S32 cols = (avail.x > cell_width) ? ((S32)avail.x / cell_width) : 1;

    F32 req_width = (cell_width * cols + style.ItemSpacing.x * 2.0f);
    F32 start = (avail.x - req_width) / 2.0f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start);
    ImGui::BeginChild("Grid", {req_width, 0});

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

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
                Image *img = &images[image_id];
                Atlas atl = atlases[img->atlas_id];

                F32 x = (F32)(img->atlas_idx % THUMB_PER_SIDE);
                F32 y = (F32)(img->atlas_idx / THUMB_PER_SIDE);

                if (ImGui::ImageButton(
                        "##ImgBtn",
                        atl.tex, {thumbnail_size, thumbnail_size}, {x / THUMB_PER_SIDE, y / THUMB_PER_SIDE}, {(x + 1) / THUMB_PER_SIDE, (y + 1) / THUMB_PER_SIDE}))
                {
                    ui_state.page = UIPage_PREVIEW;
                    ui_preview.image_id = image_id;

                    ui_preview.render_args = {
                        .texture = &ui_preview.texture,
                        .path = img->path};
                    AsyncTask task = {
                        .func = gl_tex_path,
                        .data = {
                            .kind = TPData_ANY,
                            .val_any = &ui_preview.render_args}};
                    threadpool_enqueue(TaskPriority_Realtime, task);
                }
                if (ImGui::BeginItemTooltip())
                {
                    ImGui::Text("Filename: %.*s", StringSpr(img->filename));
                    ImGui::Text("Size: %.2f%s", img->size.value, CStrCast(byte_string(img->size.unit)));
                    ImGui::EndTooltip();
                }
            }
            else
            {
                ImGui::ImageButton(
                    "##ImgEmpty",
                    (ImTextureID)0, {(F32)thumbnail_size, (F32)thumbnail_size});
            }

            total_rendered++;
            if (total_rendered % cols != 0)
                ImGui::SameLine();

            ImGui::PopID();
        }
        ImGui::NewLine();
    }

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
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
    if (ImGui::BeginCombo("##order", sort_options[ui_state.view_query.sort_basis].text, ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
    {
        for (S32 n = 0; n < StaticArrSize(sort_options); n++)
        {
            B32 is_selected = (ui_state.view_query.sort_basis == n);
            if (ImGui::Selectable(sort_options[n].text, is_selected))
            {
                mscbl_config.view_settings.sort_basis = (SortType)n;
                ui_state.view_query.sort_basis = (SortType)n;
                view_clear_state();
                view_set_state(ui_state.view_query);
                view_reload();
            }
            if (is_selected)
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

void vertical_bar(F32 width, ImVec2 window_padding, ImVec4 color)
{
    ImVec2 start_pos = ImGui::GetCursorScreenPos();
    start_pos.x -= window_padding.x;
    start_pos.y -= window_padding.y;
    ImVec2 end_pos = ImVec2(start_pos.x + width, start_pos.y + ImGui::GetFrameHeight() + window_padding.y * 2.0f);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(start_pos, end_pos, ImGui::ColorConvertFloat4ToU32(color));
    ImGui::Dummy(ImVec2(width, 0.0f));
    ImGui::SameLine();
}

void filters()
{
    F32 width_total = ImGui::GetContentRegionAvail().x;
    F32 width_left = width_total;

    B32 first_item = 1;
    UIFilter *filters = ui_state.view_query.filters;
    for (UIFilter *filter = filters; filter != NULL; filter = filter->next)
    {
        if (!filter->active)
            continue;

        F32 expected_width = 0.0f;
        expected_width += ImGui::CalcTextSize(filter_text(filter->type)).x;
        expected_width += ImGui::CalcTextSize(ICON_LC_X).x;
        switch (filter->type)
        {
        case FilterType_Path:
            expected_width += ImGui::CalcTextSize((filter->exclude) ? (ICON_LC_CIRCLE_MINUS) : (ICON_LC_CIRCLE_PLUS)).x;
            expected_width += SPACING(100.0f);
            break;
        case FilterType_SizeGreater:
            expected_width += ImGui::CalcTextSize((filter->exclude) ? (ICON_LC_CHEVRON_LEFT) : (ICON_LC_CHEVRON_RIGHT)).x;
            expected_width += SPACING(16.0f);
            expected_width += SPACING(6.0f);
            break;
        case FilterType_DateCreatedAfter:
        case FilterType_DateModifiedAfter:
            expected_width += ImGui::CalcTextSize((filter->exclude) ? (ICON_LC_CHEVRON_LEFT) : (ICON_LC_CHEVRON_RIGHT)).x;
            expected_width += SPACING(16.0f);
            expected_width += SPACING(11.0f);
            break;
        case FilterType_EmbeddingDistanceGreater:
            expected_width += ImGui::CalcTextSize((filter->exclude) ? (ICON_LC_CHEVRON_LEFT) : (ICON_LC_CHEVRON_RIGHT)).x;
            expected_width += SPACING(30.0f);
            break;
        default: break;
        }
        expected_width += SPACING(10.0f);

        F32 used_width = width_left + expected_width;
        if (used_width < width_total)
        {
            ImGui::SameLine();
            width_left = used_width;
        }
        else
        {
            width_left = expected_width;
        }

        ImGui::PushID((U64)filter);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));

        DeferLoop(ImGui::BeginChild((ImGuiID)(U64)filter, ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY), ImGui::EndChild())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING));
            vertical_bar(MSCBL_INNER_PADDING, ImVec2(MSCBL_INNER_PADDING, MSCBL_INNER_PADDING), filter->exclude ? MSCBL_STATE_SUBTRACTIVE : MSCBL_STATE_ADDITIVE);

            if (ImGui::BeginCombo("##FilterType", filter_text(filter->type), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
            {
                for (U32 i = 0; i < FilterType_COUNT; i++)
                {
                    B32 is_selected = (filter->type == (FilterType)i);
                    if (ImGui::Selectable(filter_text((FilterType)i), is_selected))
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
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::PushStyleColor(ImGuiCol_Button, COLOR_TRANSPARENT);
            ImGui::SameLine();
            switch (filter->type)
            {
            case FilterType_Path:
                if (ImGui::Button((filter->exclude) ? (ICON_LC_CIRCLE_MINUS) : (ICON_LC_CIRCLE_PLUS)))
                    filter->exclude = !filter->exclude;
                break;
            case FilterType_SizeGreater:
            case FilterType_DateCreatedAfter:
            case FilterType_DateModifiedAfter:
            case FilterType_EmbeddingDistanceGreater:
                if (ImGui::Button((filter->exclude) ? (ICON_LC_CHEVRON_LEFT) : (ICON_LC_CHEVRON_RIGHT)))
                    filter->exclude = !filter->exclude;
                break;
            default: break;
            }
            ImGui::PopStyleColor();

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
                        B32 is_selected = (filter->val_bytes.unit == (ByteUnit)i);
                        if (ImGui::Selectable(CStrCast(byte_string((ByteUnit)i)), is_selected))
                        {
                            filter->val_bytes.unit = (ByteUnit)i;
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
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
                        B32 is_selected = (filter->val_date.month == (Month)i);
                        if (ImGui::Selectable(CStrCast(month_string((Month)i)), is_selected))
                        {
                            filter->val_date.month = (Month)i;
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
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
            // ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 1.0f);
            // ImGui::SameLine();
            vertical_bar(1.0f, ImVec2(0, MSCBL_INNER_PADDING), MSCBL_BORDER);

            ImGui::SameLine();
            if (ImGui::Button(ICON_LC_X))
            {
                filter->active = 0;
            }
            ImGui::PopStyleVar();
        }

        ImGui::PopStyleVar(4);

        ImGui::PopID();
    }
}

void top_control_bar()
{
    ImGuiStyle &style = ImGui::GetStyle();
    F32 search_width = ImGui::GetContentRegionAvail().x;

    // subtract spacing/padding
    search_width -= 5.0f * style.ItemSpacing.x;
    search_width -= 12.0f * style.FramePadding.x;

    // subtract button sizes
    search_width -= ImGui::CalcTextSize("Search").x;
    search_width -= ImGui::CalcTextSize("Clear").x;
    search_width -= ImGui::CalcTextSize(zoom_options[zoom_index].text).x;
    search_width -= ImGui::CalcTextSize(sort_options[ui_state.view_query.sort_basis].text).x;
    search_width -= ImGui::CalcTextSize(ICON_LC_CALENDAR_ARROW_UP).x;
    search_width -= ImGui::CalcTextSize(ICON_LC_LIST_FILTER_PLUS " Add Filter").x;

    // extra safety space
    search_width -= SPACING(5.0f);

    DeferLoop(ImGui::BeginGroup(), ImGui::EndGroup())
    {
        ImGui::SetNextItemWidth(search_width);
        StringBuilder *buf_p = &ui_state.view_query.search_query;
        B32 do_search = ImGui::InputTextWithHint("##Search", "Search...", CStrCast(ui_state.view_query.search_query), 4096, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit, text_callback, &buf_p);

        ImGui::SameLine();
        if (ImGui::Button("Search") || do_search)
        {
            view_clear_state();
            view_set_state(ui_state.view_query);
            view_reload();
        }

        ImGui::SameLine(0.0f, 0.0f);
        if (ImGui::Button("Clear"))
        {
            ui_viewquery_clear();
            view_clear_state();
            view_reload();
        }
    }

    ImGui::SameLine();
    zoom_controls();

    ImGui::SameLine();
    sort_controls();

    ImGui::SameLine(0.0f, 0.0f);
    const char *icon = NULL;
    switch (ui_state.view_query.sort_basis)
    {
    case SortType_Path:
    case SortType_Filename:
        icon = (ui_state.view_query.descending) ? (ICON_LC_ARROW_UP_Z_A) : (ICON_LC_ARROW_DOWN_A_Z);
        break;
    case SortType_Size:
        icon = (ui_state.view_query.descending) ? (ICON_LC_ARROW_UP_1_0) : (ICON_LC_ARROW_DOWN_0_1);
        break;
    case SortType_DateCreated:
    case SortType_DateModified:
        icon = (ui_state.view_query.descending) ? (ICON_LC_CALENDAR_ARROW_DOWN) : (ICON_LC_CALENDAR_ARROW_UP);
        break;
    default: break;
    }
    if (ImGui::Button(icon))
    {
        B32 switched_direction = !ui_state.view_query.descending;
        mscbl_config.view_settings.descending = switched_direction;
        ui_state.view_query.descending = switched_direction;
        view_clear_state();
        view_set_state(ui_state.view_query);
        view_reload();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_LC_LIST_FILTER_PLUS " Add Filter"))
    {
        ui_add_filter();
    }

    filters();
}

void menu_draw_docked_main(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));
    ImGui::Begin("MainPanel", NULL, flags);

    top_control_bar();

    ImGui::NewLine();
    main_grid();

    ImGui::End();
    ImGui::PopStyleVar();
}

void menu_draw_docked_status(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_INNER_PADDING));

    ImGui::Begin("StatusPanel", NULL, flags);

    ImGui::Text("All tasks done");

    ImGui::End();
    ImGui::PopStyleVar();
}

MSCBL_EXP void page_menu()
{
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (last_work_size.x != viewport->WorkSize.x || last_work_size.y != viewport->WorkSize.y)
        needs_rebuild = 1;
    last_work_size = viewport->WorkSize;

    F32 sidebar_width = sidebar_open ? SPACING(sidebar_open_units) : SPACING(sidebar_fold_units);

    if (ImGui::DockBuilderGetNode(dockspace_id) == NULL || needs_rebuild)
    {
        needs_rebuild = 0;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id);
        ImGui::DockBuilderSetNodeSize(dockspace_id, last_work_size);

        ImGuiID dock_main_id = dockspace_id;

        F32 sidebar_ratio = sidebar_width / last_work_size.x;
        ImGuiID dock_id_sidebar = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, sidebar_ratio, NULL, &dock_main_id);

        F32 target_status_height = ImGui::GetFrameHeight();
        F32 status_bar_ratio = target_status_height / last_work_size.y;
        ImGuiID dock_id_status = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, status_bar_ratio, NULL, &dock_main_id);

        ImGui::DockBuilderDockWindow("SidebarPanel", dock_id_sidebar);
        ImGui::DockBuilderDockWindow("StatusPanel", dock_id_status);
        ImGui::DockBuilderDockWindow("MainPanel", dock_main_id);

        ImGui::DockBuilderFinish(dockspace_id);
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

    menu_draw_docked_sidebar(window_flags);
    menu_draw_docked_main(window_flags);
    menu_draw_docked_status(window_flags);
}
