#include "base/base_core.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "ui/pages/menu/menu.h"
#include "db/view.h"
#include "ui/theme.h"
#include "ui/ui_core.h"
#include "base/array.h"
#include "base/string.h"

enum GridType
{
    GridType_Default,
    GridType_Result,
};

// struct GridMap
// {
//     // What db returns
//     StringArr headers;
//     S64 *item_counts;
//
//     // What I generate
//     S64 *global_group_offset;
//     F32 *y_offset;
// };
//
// struct Range
// {
//     S64 start;
//     S64 end;
// };

local_v F32 scroll_distance = 0;
// local_v Range visible_range = {0};
// local_v GridMap map = {.headers = 0};

S64 search_lower(F32 *array, F32 target, S64 start = 0, S64 end = -1)
{
    S64 low = start;
    S64 high = (end == -1) ? da_getsize(array) - 1 : end;
    S64 result = high;

    while (low < high)
    {
        S64 mid = low + (high - low) / 2;
        if (array[mid] >= target)
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

S64 search_upper(F32 *array, F32 target, S64 start = 0, S64 end = -1)
{
    S64 low = start;
    S64 high = (end == -1) ? da_getsize(array) - 1 : end;
    S64 result = high;

    while (low < high)
    {
        S64 mid = low + (high - low) / 2;
        if (array[mid] > target)
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

// Range get_visible_range(F32 scroll_start, F32 scroll_end)
// {
//     S64 start = search_lower(map.y_offset, scroll_start);
//     S64 end = search_upper(map.y_offset, scroll_end, start);
//     return {.start = start, .end = end};
// }

void menu_render_grid()
{
    // Generate dummy data
    // if (da_getsize(map.headers) <= 0)
    // {
    //     for (U32 i = 1; i < 101; i++)
    //     {
    //         da_push(ui_state.view_query.arena, map.headers, sv("something"));
    //         da_push(ui_state.view_query.arena, map.item_counts, (S64)i);
    //     }
    //
    //     S64 i = 10;
    //     da_push(ui_state.view_query.arena, map.headers, sv("last"));
    //     da_push(ui_state.view_query.arena, map.item_counts, i);
    // }

    ImGuiStyle &style = ImGui::GetStyle();
    F32 thumbnail_size = zoom_options[zoom_level].size;

    F32 table_cell_padding_y = 1.0f;
    F32 row_height = ImFloor(thumbnail_size + (table_cell_padding_y * 2.0f));

    ImGui::PushFont(ui_state.title_font);
    F32 text_height = ImCeil(ImGui::GetTextLineHeight());
    ImGui::PopFont();
    F32 header_height = text_height + style.ItemSpacing.y * 2.0f;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    F32 cell_width = thumbnail_size;
    S32 cols = (avail.x > cell_width) ? ((S32)avail.x / cell_width) : 1;

    // Recalculate y_offset array
    S64 header_count = 0;
    switch (ui_state.view_query.sort_basis)
    {
    case SortType_Directory:
    case SortType_Filename:
        header_count = da_getsize(ui_search.main.str_headers);
        break;
    case SortType_Size:
        header_count = da_getsize(ui_search.main.size_headers);
        break;
    case SortType_DateAdded:
    case SortType_DateCreated:
    case SortType_DateModified:
        header_count = da_getsize(ui_search.main.time_headers);
        break;
    default:
        break;
    }
    if (da_getsize(ui_search.main.y_offset) < header_count + 1 || da_getsize(ui_search.main.global_group_offset) < header_count + 1)
    {
        S64 global_offset = 0;
        F32 y_offset = 0;
        for (S64 i = 0; i < header_count; i++)
        {
            da_push(ui_state.view_query.arena, ui_search.main.global_group_offset, global_offset);
            global_offset += ui_search.main.item_counts[i];

            da_push(ui_state.view_query.arena, ui_search.main.y_offset, y_offset);

            S64 group_rows = ToCeilInt(ui_search.main.item_counts[i], cols);
            F32 group_height = header_height + (group_rows * row_height) + (style.ItemSpacing.y * 2.0f);
            y_offset += group_height;
        }
        // Guard element containing overall dynamic height
        da_push(ui_state.view_query.arena, ui_search.main.global_group_offset, global_offset);
        da_push(ui_state.view_query.arena, ui_search.main.y_offset, y_offset);
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_TRANSPARENT);
    DeferLoop(ImGui::BeginChild("Grid"), ImGui::EndChild())
    {
        F32 scroll_window_start = ImGui::GetScrollY();
        F32 scroll_window_end = scroll_window_start + ImGui::GetWindowSize().y;

        // Fetch currently visible group index range
        if (ToAbs(scroll_window_start - scroll_distance) >= zoom_options[zoom_level].size)
        {
            S64 group_start = search_lower(ui_search.main.y_offset, scroll_window_start);
            S64 group_end = search_upper(ui_search.main.y_offset, scroll_window_end, group_start);
        }

        ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(1.0f, table_cell_padding_y));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        // Record start Y of child content region
        F32 start_cursor_y = ImGui::GetCursorPosY();

        for (S64 g = 0; g < header_count; g++)
        {
            // Calculate starting & ending dynamic vertical positions for group `g`
            F32 group_y_start = ui_search.main.y_offset[g];
            F32 group_y_end = ui_search.main.y_offset[g + 1];

            // CULLING / SKIPPING CHECK:
            // Skip rendering widgets if group completely falls outside the view boundary
            if (group_y_end < scroll_window_start || group_y_start > scroll_window_end)
            {
                continue;
            }

            // Reposition cursor directly to group start offset for visible items
            ImGui::SetCursorPosY(start_cursor_y + group_y_start);

            S64 total_items = ui_search.main.item_counts[g];

            // 1. Render Group Header
            ImGui::PushFont(ui_state.title_font);
            switch (ui_state.view_query.sort_basis)
            {
            case SortType_Directory:
            case SortType_Filename:
                ImGui::Text("%.*s \t(%zu items)", StringSpr(ui_search.main.str_headers[g]), total_items);
                break;
            case SortType_Size: {
                ByteSize size = ui_search.main.size_headers[g];
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
                    Time date = ui_search.main.time_headers[g];
                    Time next_day = skip_days(date, 1);
                    ImGui::Text("%d-%d %.*s %d", date.date, next_day.date, StringSpr(month_string(date.month)), date.year);
                }
                break;
                case Group_DateMonth: {
                    Time date = ui_search.main.time_headers[g];
                    Time next_month = skip_months(date, 1);
                    ImGui::Text("%.*s-%.*s %d", StringSpr(month_string(date.month)), StringSpr(month_string(next_month.month)), date.year);
                }
                break;
                case Group_DateYear: {
                    Time date = ui_search.main.time_headers[g];
                    Time next_year = skip_years(date, 1);
                    ImGui::Text("%d-%d", date.year, next_year.year);
                }
                break;
                default: break;
                }
                break;
            default: break;
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

                S64 group_start_idx = ui_search.main.global_group_offset[g];

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

                        ImGui::PushID((int)global_item_idx);
                        if (ImGui::Button("...", ImVec2(thumbnail_size, thumbnail_size)))
                        {
                            view_fetch_map();
                        }
                        ImGui::PopID();
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
        F32 total_content_height = ui_search.main.y_offset[header_count];
        ImGui::SetCursorPosY(start_cursor_y + total_content_height);

        // Submit a dummy item to explicitly notify ImGui of the expanded boundaries
        ImGui::Dummy(ImVec2(0.0f, 0.0f));

        ImGui::PopStyleVar(2);
    }
    ImGui::PopStyleColor();
}

void menu_grid()
{
    switch (ui_state.view_query.query_type)
    {
    case QueryType_COUNT:
    case QueryType_Default:
        menu_render_grid();
        break;
    default:
        DeferLoop(ImGui::BeginTabBar("Tabs"), ImGui::EndTabBar())
        {
            if (ImGui::BeginTabItem("Text Search"))
            {
                ui_state.view_query.query_type = QueryType_FTS;
                menu_render_grid();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Context Search"))
            {
                ui_state.view_query.query_type = QueryType_Embedding;
                menu_render_grid();
                ImGui::EndTabItem();
            }
            break;
        }
    }
}
