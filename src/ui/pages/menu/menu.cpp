// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "imgui.h"
#include "IconsLucide.h"

#include "ui/pages/pages.h"
#include "ui/pages/menu/menu.h"
#include "ui/pages/menu/grid.cpp"
#include "ui/pages/menu/sidebar.cpp"

#include "config.h"
#include "db/view.h"
#include "db/fetch.h"
#include "base/log.h"
#include "ui/theme.h"
#include "scan/scan.h"
#include "base/array.h"
#include "ui/ui_core.h"
#include "app/miscible.h"
#include "base/base_core.h"

void zoom_controls()
{
    if (ImGui::BeginCombo("##zoom", zoom_options[zoom_level].text, ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
    {
        for (S32 n = 0; n < Zoom_COUNT; n++)
        {
            ZoomOption option = zoom_options[n];
            B32 is_selected = (zoom_level == n);
            if (ImGui::Selectable(option.text, is_selected))
            {
                zoom_level = option.level;
                recompute_layout();
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
        for (S32 n = 0; n < SortType_COUNT; n++)
        {
            SortOption option = sort_options[n];
            B32 is_selected = (ui_state.view_query.sort_basis == option.type);
            if (ImGui::Selectable(option.text, is_selected))
            {
                mscbl_config.view_settings.sort_basis = option.type;
                ui_state.view_query.sort_basis = option.type;
                if (option.type == SortType_Size)
                {
                    ui_state.view_query.sub_type = Group_Size1KB;
                }
                if (option.type == SortType_DateModified || option.type == SortType_DateCreated || option.type == SortType_DateAdded)
                {
                    ui_state.view_query.sub_type = Group_DateMonth;
                }

                view_fetch_new();
            }
            if (is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void menu_draw_docked_topbar(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));
    DeferLoop(ImGui::Begin("TopbarPanel", NULL, flags), ImGui::End())
    {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec2 content_max = ImGui::GetWindowContentRegionMax();
        ImVec2 content_min = ImGui::GetWindowContentRegionMin();
        F32 total_width = content_max.x - content_min.x;

        ImVec2 avail = ImGui::GetContentRegionAvail();
        F32 start_y = ImGui::GetCursorPosY();

        // 1. Draw Icon
        F32 image_size = 32.0f;
        F32 image_y = start_y + (avail.y - image_size) * 0.5f;
        ImGui::SetCursorPosY(image_y);
        ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 0.0f);
        ImGui::Image(ui_state.icon_texture, ImVec2(image_size, image_size));
        ImGui::PopStyleVar();
        ImGui::SameLine();

        // 2. Draw Title Text
        ImGui::PushFont(ui_state.display_font);
        F32 text_height = ImGui::CalcTextSize(Stringify(APP_NAME)).y;
        F32 text_y = start_y + (avail.y - text_height) * 0.5f;
        ImGui::SetCursorPosY(text_y);
        ImGui::Text(" " Stringify(APP_NAME_DISPLAY));
        ImGui::PopFont();
        F32 current_left_x = ImGui::GetCursorPosX();

        // 3. Center and dynamically shrink the Search Bar + Controls Group
        F32 right_buttons_width = ImGui::CalcTextSize(ICON_LC_MINUS ICON_LC_MAXIMIZE ICON_LC_X).x + 6.0f * style.FramePadding.x;
        F32 settings_btn_width = ImGui::CalcTextSize(ICON_LC_SETTINGS).x + 2.0f * style.FramePadding.x;

        // Calculate sizes of the static elements in the group
        const char *search_btn_text = "Search";
        F32 search_btn_width = ImGui::CalcTextSize(search_btn_text).x + 2.0f * style.FramePadding.x;

        const char *clear_btn_text = "Clear All";
        F32 clear_btn_width = ImGui::CalcTextSize(clear_btn_text).x + 2.0f * style.FramePadding.x;

        F32 zoom_combo_width = ImGui::CalcTextSize(zoom_options[zoom_level].text).x + 2.0f * style.FramePadding.x;
        F32 sort_combo_width = (ui_state.view_query.search_type != SearchType_Embedding) ? ImGui::CalcTextSize(sort_options[ui_state.view_query.sort_basis].text).x + 2.0f * style.FramePadding.x : 0;

        // Determine the sort direction icon text to measure its dynamic button width
        const char *direction_icon = "";
        switch (ui_state.view_query.sort_basis)
        {
        case SortType_Directory:
        case SortType_Filename:
            direction_icon = (ui_state.view_query.descending) ? (ICON_LC_ARROW_UP_Z_A) : (ICON_LC_ARROW_DOWN_A_Z);
            break;
        case SortType_Size:
            direction_icon = (ui_state.view_query.descending) ? (ICON_LC_ARROW_UP_1_0) : (ICON_LC_ARROW_DOWN_0_1);
            break;
        case SortType_DateAdded:
        case SortType_DateCreated:
        case SortType_DateModified:
            direction_icon = (ui_state.view_query.descending) ? (ICON_LC_CALENDAR_ARROW_DOWN) : (ICON_LC_CALENDAR_ARROW_UP);
            break;
        default: break;
        }
        F32 direction_btn_width = ImGui::CalcTextSize(direction_icon).x + 2.0f * style.FramePadding.x;

        F32 total_spacings_width = 4.0f * style.ItemSpacing.x;

        // Combine all static items (including the sort direction button)
        F32 static_controls_width = settings_btn_width + search_btn_width + clear_btn_width + zoom_combo_width + sort_combo_width + direction_btn_width + total_spacings_width;

        // Set the minimum width the search bar is allowed to shrink to
        F32 min_search_bar_width = 150.0f;
        F32 max_group_width = 1000.0f;

        // The group width is clamped to the available window width
        F32 safety_margin = current_left_x + right_buttons_width + style.ItemSpacing.x * 2.0f;
        F32 max_available_for_group = (total_width > safety_margin) ? (total_width - safety_margin) : min_search_bar_width;

        // Group width shrinks dynamically with the window
        F32 total_group_width = max_group_width;
        if (total_group_width > max_available_for_group)
        {
            total_group_width = max_available_for_group;
        }

        // Calculate the dynamic search bar width
        F32 search_bar_width = total_group_width - static_controls_width;
        if (search_bar_width < min_search_bar_width)
        {
            search_bar_width = min_search_bar_width;
            total_group_width = static_controls_width + min_search_bar_width;
        }

        F32 search_height = ImGui::CalcTextSize("Search").y + 2.0f * style.FramePadding.y;

        // Calculate the X coordinate for the entire combined group
        F32 search_x = content_min.x + (total_width - total_group_width) * 0.5f;
        F32 search_y = start_y + (avail.y - search_height) * 0.5f;

        // Protect left elements (Title Text) from overlapping
        if (search_x < current_left_x)
        {
            search_x = current_left_x;
        }

        ImGui::SetCursorPosX(search_x);
        ImGui::SetCursorPosY(search_y);

        ImGui::SetNextItemWidth(search_bar_width);
        StringBuilder *buf_p = &ui_state.view_query.search_query;
        B32 do_search = ImGui::InputTextWithHint("##Search", search_options[ui_state.view_query.search_type].info_text, CStrCast(ui_state.view_query.search_query), 4096, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit, text_callback, &buf_p);

        ImGui::SameLine();
        if (ImGui::Button(search_btn_text, ImVec2(search_btn_width, search_height)) || do_search)
        {
            view_fetch_new();
        }

        ImGui::SameLine(0.0f, 0.0f);
        if (ImGui::Button(clear_btn_text, ImVec2(clear_btn_width, search_height)))
        {
            ui_viewquery_clear();
            view_fetch_new();
        }

        ImGui::SameLine();
        zoom_controls();

        if (ui_state.view_query.search_type != SearchType_Embedding)
        {
            ImGui::SameLine();
            sort_controls();

            ImGui::SameLine(0.0f, 0.0f);
            if (ImGui::Button(direction_icon, ImVec2(direction_btn_width, search_height)))
            {
                B32 switched_direction = !ui_state.view_query.descending;
                mscbl_config.view_settings.descending = switched_direction;
                ui_state.view_query.descending = switched_direction;

                view_fetch_new();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_LC_SETTINGS, ImVec2(settings_btn_width, search_height)))
        {
            ui_push_message({.success = 0, .domain = Domain_App, .code = 1, .context = "implementation pending"});
            // TODO: this
        }
    }
    ImGui::PopStyleVar();
}

void menu_draw_docked_sidebar(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));
    ImGui::Begin("SidebarPanel", NULL, flags);

    menu_sidebar();

    ImGui::End();
    ImGui::PopStyleVar();
}

void menu_draw_docked_main(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));
    ImGui::Begin("MainPanel", NULL, flags);

    // top_control_bar();
    ImVec2 avail = ImGui::GetContentRegionAvail();

    for (U32 i = 0; i < SearchType_COUNT; i++)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, (ui_state.view_query.search_type == i) ? MSCBL_INTERACTION_ACTIVE : MSCBL_INTERACTION_IDLE);
        ImGui::PushStyleColor(ImGuiCol_Text, (ui_state.view_query.search_type == i) ? MSCBL_FOREGROUND : MSCBL_FOREGROUND_MUTED);
        ImGui::PushStyleColor(ImGuiCol_Border, (ui_state.view_query.search_type == i) ? MSCBL_BORDER : COLOR_TRANSPARENT);
        if (ImGui::Button(search_options[i].button_text))
        {
            ui_state.view_query.search_type = (SearchType)i;
            view_fetch_new();
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        ImGui::SameLine(0, 0);
    }

    ImGuiStyle &style = ImGui::GetStyle();
    F32 subgroup_button_width = 0.0f;
    switch (ui_state.view_query.sort_basis)
    {
    case SortType_Size:
        subgroup_button_width += ImGui::CalcTextSize("1 KB10 KB100 KB1 MB10 MB100 MB1 GB").x;
        subgroup_button_width += StaticArrSize(size_group_options) * style.FramePadding.x * 2.0f;
        break;
    case SortType_DateAdded:
    case SortType_DateCreated:
    case SortType_DateModified:
        subgroup_button_width += ImGui::CalcTextSize("DayMonthYear").x;
        subgroup_button_width += StaticArrSize(date_group_options) * style.FramePadding.x * 2.0f;
        break;
    default:
        break;
    }

    ImGui::SetCursorPosX(avail.x - subgroup_button_width);
    switch (ui_state.view_query.sort_basis)
    {
    case SortType_Size:
        for (S32 n = 0; n < StaticArrSize(size_group_options); n++)
        {
            GroupOption option = size_group_options[n];
            B32 is_selected = (ui_state.view_query.sub_type == option.type);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, (is_selected) ? MSCBL_INTERACTION_ACTIVE : MSCBL_INTERACTION_IDLE);
            ImGui::PushStyleColor(ImGuiCol_Text, (is_selected) ? MSCBL_FOREGROUND : MSCBL_FOREGROUND_MUTED);
            ImGui::PushStyleColor(ImGuiCol_Border, (is_selected) ? MSCBL_BORDER : COLOR_TRANSPARENT);
            if (ImGui::Button(option.text))
            {
                ui_state.view_query.sub_type = option.type;
                view_fetch_new();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            ImGui::SameLine(0, 0);
        }
        break;
    case SortType_DateAdded:
    case SortType_DateCreated:
    case SortType_DateModified:
        for (S32 n = 0; n < StaticArrSize(date_group_options); n++)
        {
            GroupOption option = date_group_options[n];
            B32 is_selected = (ui_state.view_query.sub_type == option.type);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, (is_selected) ? MSCBL_INTERACTION_ACTIVE : MSCBL_INTERACTION_IDLE);
            ImGui::PushStyleColor(ImGuiCol_Text, (is_selected) ? MSCBL_FOREGROUND : MSCBL_FOREGROUND_MUTED);
            ImGui::PushStyleColor(ImGuiCol_Border, (is_selected) ? MSCBL_BORDER : COLOR_TRANSPARENT);
            if (ImGui::Button(option.text))
            {
                ui_state.view_query.sub_type = option.type;
                view_fetch_new();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            ImGui::SameLine(0, 0);
        }
        break;
    default:
        break;
    }

    ImGui::NewLine();
    menu_grid();

    ImGui::End();
    ImGui::PopStyleVar();
}

void menu_draw_docked_status(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_INNER_PADDING));

    DeferLoop(ImGui::Begin("StatusPanel", NULL, flags), ImGui::End())
    {
        ImGui::Text("All tasks done");
    }

    ImGui::PopStyleVar();
}

MSCBL_EXP void page_menu()
{
    switch_page = 0;
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    if (last_work_size.x != viewport->WorkSize.x || last_work_size.y != viewport->WorkSize.y)
    {
        needs_rebuild = 1;
    }
    last_work_size = viewport->WorkSize;

    F32 sidebar_width = sidebar_open ? SPACING(sidebar_open_units) : SPACING(sidebar_fold_units);

    if (ImGui::DockBuilderGetNode(dockspace_id) == NULL || needs_rebuild)
    {
        needs_rebuild = 0;
        recompute_layout();

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id);
        ImGui::DockBuilderSetNodeSize(dockspace_id, last_work_size);

        ImGuiID dock_main_id = dockspace_id;

        F32 topbar_ratio = SPACING(topbar_height_units) / last_work_size.y;
        ImGuiID dock_id_topbar = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, topbar_ratio, NULL, &dock_main_id);

        F32 target_status_height = ImGui::GetFrameHeight();
        F32 status_bar_ratio = target_status_height / last_work_size.y;
        ImGuiID dock_id_status = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, status_bar_ratio, NULL, &dock_main_id);

        F32 sidebar_ratio = sidebar_width / last_work_size.x;
        ImGuiID dock_id_sidebar = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, sidebar_ratio, NULL, &dock_main_id);

        ImGui::DockBuilderDockWindow("TopbarPanel", dock_id_topbar);
        ImGui::DockBuilderDockWindow("StatusPanel", dock_id_status);
        ImGui::DockBuilderDockWindow("SidebarPanel", dock_id_sidebar);
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

    menu_draw_docked_topbar(window_flags);
    menu_draw_docked_status(window_flags);
    menu_draw_docked_sidebar(window_flags);
    menu_draw_docked_main(window_flags);

    if (switch_page)
    {
        needs_rebuild = 1;
        ui_state.page = UIPage_PREVIEW;
    }
}
