// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "imgui.h"
#include "imgui_internal.h"

#include "ui/pages/menu/menu.h"
#include "db/view.h"
#include "ui/pages/pages.h"
#include "ui/theme.h"
#include "ui/ui_core.h"
#include "base/array.h"
#include "base/string.h"
#include "app/miscible.h"

enum GridType
{
    GridType_Default,
    GridType_Result,
};

local_v F32 scroll_distance = 0;

void recompute_layout()
{
    ui_result.main_map.y_offset_computed = 0;
}

S64 search_lower(UIMap *map, F32 target, S64 start = 0, S64 end = -1)
{
    S64 low = start;
    S64 high = (end == -1) ? arr_getsize(map) : end;
    S64 result = high;

    while (low < high)
    {
        S64 mid = low + (high - low) / 2;
        if (map[mid].y_offset >= target)
        {
            result = mid;
            high = mid;
        }
        else
        {
            low = mid + 1;
        }
    }
    return result;
}

S64 search_upper(UIMap *map, F32 target, S64 start = 0, S64 end = -1)
{
    S64 low = start;
    S64 high = (end == -1) ? arr_getsize(map) : end;
    S64 result = high;

    while (low < high)
    {
        S64 mid = low + (high - low) / 2;
        if (map[mid].y_offset > target)
        {
            result = mid;
            high = mid;
        }
        else
        {
            low = mid + 1;
        }
    }
    return result;
}

void menu_grid()
{
    UIMapResult *map = &ui_result.main_map;
    UIMetadataResult *list = &ui_result.main_list;
    S64 header_count = arr_getsize(map->map) - 1;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGuiStyle &style = ImGui::GetStyle();

    F32 thumbnail_size = zoom_options[zoom_level].size;

    F32 table_cell_padding_y = 1.0f;
    F32 row_height = ImFloor(thumbnail_size + (table_cell_padding_y * 2.0f));

    ImGui::PushFont(ui_state.title_font);
    F32 text_height = ImCeil(ImGui::GetTextLineHeight());
    ImGui::PopFont();
    F32 header_height = text_height + style.ItemSpacing.y * 2.0f;

    F32 cell_width = thumbnail_size;
    S32 cols = (avail.x > cell_width) ? ((S32)avail.x / cell_width) : 1;

    if (map->map)
    {
        if (!map->y_offset_computed)
        {
            S64 i = 0;
            F32 y_offset = 0;
            for (; i < header_count; i++)
            {
                map->map[i].y_offset = y_offset;
                S64 group_rows = ToCeilInt(map->map[i].item_count, cols);
                F32 group_height = header_height + (group_rows * row_height) + (style.ItemSpacing.y * 2.0f);
                y_offset += group_height;
            }
            // Guard element containing overall dynamic height
            map->map[i].y_offset = y_offset;
            map->y_offset_computed = 1;
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_TRANSPARENT);
        DeferLoop(ImGui::BeginChild("Grid"), ImGui::EndChild())
        {
            F32 scroll_window_start = ImGui::GetScrollY();
            F32 scroll_window_end = scroll_window_start + ImGui::GetWindowSize().y;

            // Fetch currently visible group index range
            if (ToAbs(scroll_window_start - scroll_distance) >= zoom_options[zoom_level].size)
            {
                S64 max_idx = arr_getsize(map->map) - 1;

                S64 group_start = search_lower(map->map, scroll_window_start);
                S64 group_end = search_upper(map->map, scroll_window_end, group_start);

                if (group_start == group_end)
                {
                    group_start--;
                    group_end++;
                }

                if (group_start < 0) group_start = 0;
                if (group_end < 0) group_end = 0;

                if (group_start >= max_idx) group_start = max_idx;
                if (group_end >= max_idx) group_end = max_idx;

                view_fetch_images(map->map[group_start].global_group_offset, map->map[group_end].global_group_offset);
                scroll_distance = scroll_window_start;
            }

            ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(1.0f, table_cell_padding_y));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

            // Record start Y of child content region
            F32 start_cursor_y = ImGui::GetCursorPosY();

            for (S64 g = 0; g < header_count; g++)
            {
                // Calculate starting & ending dynamic vertical positions for group `g`
                F32 group_y_start = map->map[g].y_offset;
                F32 group_y_end = map->map[g + 1].y_offset;

                // CULLING / SKIPPING CHECK:
                // Skip rendering widgets if group completely falls outside the view boundary
                if (group_y_end < scroll_window_start || group_y_start > scroll_window_end)
                {
                    continue;
                }

                // Reposition cursor directly to group start offset for visible items
                ImGui::SetCursorPosY(start_cursor_y + group_y_start);

                S64 total_items = map->map[g].item_count;

                // 1. Render Group Header
                ImGui::PushFont(ui_state.title_font);
                if (map->query_type == ViewQuery_Embedding)
                {
                    ImGui::Text("%d-%d%% Match", map->map[g].s32_header, map->map[g].s32_header + 10);
                }
                else
                {
                    switch (ui_state.view_query.sort_basis)
                    {
                    case SortType_Directory:
                    case SortType_Filename:
                        ImGui::Text("%.*s", StringSpr(map->map[g].str_header));
                        break;
                    case SortType_Size: {
                        ByteSize size = map->map[g].size_header;
                        ByteSize next_size = {0};
                        switch (ui_state.view_query.sub_type)
                        {
                        case Group_Size1KB:
                        case Group_Size10KB:
                        case Group_Size100KB:
                        case Group_Size1MB:
                        case Group_Size10MB:
                        case Group_Size100MB:
                        case Group_Size1GB:
                            next_size = size_to_bytesize(bytesize_to_size(size) + ui_state.view_query.sub_type);
                        default: break;
                        }
                        ImGui::Text("%.0f%.*s-%.0f%.*s", size.value, StringSpr(byte_string(size.unit)), next_size.value, StringSpr(byte_string(next_size.unit)));
                    }
                    break;
                    case SortType_DateAdded:
                    case SortType_DateCreated:
                    case SortType_DateModified:
                        switch (ui_state.view_query.sub_type)
                        {
                        case Group_DateDay: {
                            Time date = map->map[g].time_header;
                            Time next_day = skip_days(date, 1);
                            ImGui::Text("%d-%d %.*s %d", date.date, next_day.date, StringSpr(month_string(date.month)), date.year);
                        }
                        break;
                        case Group_DateMonth: {
                            Time date = map->map[g].time_header;
                            Time next_month = skip_months(date, 1);
                            ImGui::Text("%.*s-%.*s %d", StringSpr(month_string(date.month)), StringSpr(month_string(next_month.month)), date.year);
                        }
                        break;
                        case Group_DateYear: {
                            Time date = map->map[g].time_header;
                            Time next_year = skip_years(date, 1);
                            ImGui::Text("%d-%d", date.year, next_year.year);
                        }
                        break;
                        default: break;
                        }
                        break;
                    default: break;
                    }
                }
                ImGui::PopFont();

                // 2. Render Group Grid Table
                ImGui::PushID((int)g);
                if (ImGui::BeginTable("GroupGridTable", cols, table_flags))
                {
                    for (S32 c = 0; c < cols; c++)
                    {
                        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, thumbnail_size);
                    }

                    S64 group_start_idx = map->map[g].global_group_offset;

                    // Item-level virtualization inside active visible groups:
                    // Find visible row range inside this table
                    F32 table_top = ImGui::GetCursorPosY();
                    S64 total_rows = ToCeilInt(total_items, cols);

                    S64 start_row = (S64)ImMax(0.0f, ImFloor((scroll_window_start - table_top) / row_height));
                    S64 end_row = (S64)ImMin((F32)total_rows, ImCeil((scroll_window_end - table_top) / row_height) + 1.0f);

                    // Skip unrendered rows prior to visible table area
                    if (start_row > 0)
                    {
                        ImGui::TableNextRow(0, start_row * row_height);
                    }

                    // Render only active rows
                    for (S64 r = start_row; r < end_row; r++)
                    {
                        ImGui::TableNextRow(0, row_height);

                        for (S32 c = 0; c < cols; c++)
                        {
                            S64 item_in_group = r * cols + c;
                            if (item_in_group >= total_items) break;

                            ImGui::TableSetColumnIndex(c);
                            S64 global_item_idx = group_start_idx + item_in_group;

                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                            S64 image_idx = global_item_idx - ui_state.view_query.visible_start;
                            if (image_idx >= 0 && arr_getsize(list->images) > image_idx && arr_getsize(fetch_atlases) > list->images[image_idx].atlas_id)
                            {
                                ImageMetadata *image = &list->images[image_idx];
                                AtlasMap atlas = fetch_atlases[image->atlas_id];

                                F32 x = (F32)(image->atlas_idx % THUMB_PER_SIDE);
                                F32 y = (F32)(image->atlas_idx / THUMB_PER_SIDE);

                                ImGui::PushID((int)global_item_idx);
                                if (ImGui::ImageButton("##Image", atlas.tex, ImVec2(thumbnail_size, thumbnail_size), ImVec2(x / THUMB_PER_SIDE, y / THUMB_PER_SIDE), ImVec2((x + 1) / THUMB_PER_SIDE, (y + 1) / THUMB_PER_SIDE)))
                                {
                                    switch_page = 1;
                                    ui_preview_image(image->id);
                                }
                                ImGui::PopID();
                            }
                            else
                            {
                                ImGui::PushID((int)global_item_idx);
                                ImGui::Button("Unavailable", ImVec2(thumbnail_size, thumbnail_size));
                                ImGui::PopID();
                            }
                            ImGui::PopStyleVar();
                        }
                    }

                    // Reserve height for remaining invisible bottom rows
                    if (end_row < total_rows)
                    {
                        S64 remaining_rows = total_rows - end_row;
                        ImGui::TableNextRow(0, remaining_rows * row_height);
                    }

                    ImGui::EndTable();
                }
                ImGui::PopID(); // group index g
            }

            // Set the final vertical position to force ImGui child container
            // to register total canvas length, maintaining the exact scrollbar size
            F32 total_content_height = map->map[header_count].y_offset;
            ImGui::SetCursorPosY(start_cursor_y + total_content_height);

            // Submit a dummy item to explicitly notify ImGui of the expanded boundaries
            ImGui::Dummy(ImVec2(0.0f, 0.0f));

            ImGui::PopStyleVar(2);
        }
        ImGui::PopStyleColor();
    }
}
