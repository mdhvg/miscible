#include "IconsLucide.h"
#include "base/base_core.h"
#include "db/view.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "ui/pages/pages.h"
#include "ui/pages/menu/menu.h"

#include "ui/theme.h"
#include "base/log.h"
#include "scan/scan.h"
#include "base/array.h"
#include "ui/ui_core.h"

local_v U32 sidebar_open = 1;
local_v B32 sidebar_dirtree_openall = 0;
local_v B32 sidebar_dirtree_foldall = 0;

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
    StringBuilderStack(sb, KB(1));
    string_format(&sb, ICON_LC_FOLDER " %.*s", StringSpr(dir.name));
    const char *dirname = format_cstr(&sb, ICON_LC_FOLDER " %.*s", StringSpr(dir.name));
    Assert(dirname, "dirname is null");

    ImGui::PushStyleColor(ImGuiCol_Button, (ui_state.view_query.selected_dir == cur)
                                               ? (MSCBL_INTERACTION_HOVER)
                                               : (COLOR_TRANSPARENT));
    if (ImGui::Button(dirname, {ImGui::GetContentRegionAvail().x, 0}))
    {
        if (ui_state.view_query.selected_dir == cur)
            ui_state.view_query.selected_dir = 0;
        else
            ui_state.view_query.selected_dir = cur;
    }
    ImGui::PopStyleColor();

    if (expand)
    {
        directory_tree(dir_tree[cur].child_idx);
        ImGui::TreePop();
    }

    directory_tree(dir_tree[cur].next_idx);
}

global_v const char *filter_text(FilterType type)
{
    switch (type)
    {
    case FilterType_SizeBetween: return "FILE SIZE";
    case FilterType_Path: return "PATH";
    case FilterType_DateAddedBetween: return "DATE ADDED";
    case FilterType_DateCreatedBetween: return "DATE CREATED";
    case FilterType_DateModifiedBetween: return "DATE MODIFIED";
    case FilterType_EmbeddingDistanceBetween: return "MATCH WITHIN";
    default: Assert(0, "unknown filter added"); return 0;
    }
}

void size_filter(UIFilter *filter)
{
}

void filters()
{
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec2 avail = ImGui::GetContentRegionAvail();

    UIFilter *filters = ui_state.view_query.filters;
    for (UIFilter *filter = filters; filter != NULL; filter = filter->next)
    {
        if (!filter->active)
            continue;

        DeferLoop(ImGui::BeginChild((U64)filter, ImVec2(avail.x, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY), ImGui::EndChild())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImVec2 right_edge = ImGui::GetContentRegionMax();
            if (ImGui::BeginCombo("##FilterType", filter_text(filter->type), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_WidthFitPreview))
            {
                for (U32 i = 0; i < FilterType_COUNT; i++)
                {
                    if (i == FilterType_EmbeddingDistanceBetween && ui_search.type != SearchType_Embedding)
                    {
                        continue;
                    }
                    B32 is_selected = (filter->type == (FilterType)i);
                    if (ImGui::Selectable(filter_text((FilterType)i), is_selected))
                    {
                        filter->type = (FilterType)i;
                        switch (filter->type)
                        {
                        case FilterType_SizeBetween:
                            filter->val_byte = {
                                .from = {.value = 0, .unit = Byte},
                                .to = {0},
                                .from_enable = 1,
                                .to_enable = 0,
                            };
                            break;
                        case FilterType_Path:
                            filter->val_str = {.val = string_empty(ui_state.view_query.arena, 4096), .exclude = 0};
                            break;
                        case FilterType_DateAddedBetween:
                        case FilterType_DateCreatedBetween:
                        case FilterType_DateModifiedBetween:
                            filter->val_time = {
                                .from = {.date = 22, .month = Month_Apr, .year = 2025},
                                .to = {.date = 7, .month = Month_May, .year = 2025},
                                .from_enable = 1,
                                .to_enable = 1,
                            };
                            break;
                        case FilterType_EmbeddingDistanceBetween:
                            filter->val_float = {
                                .from = 90,
                                .to = 100,
                                .from_enable = 1,
                                .to_enable = 1};
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

            ImGui::SameLine();
            ImGui::SetCursorPosX(right_edge.x - ImGui::CalcTextSize(ICON_LC_X).x - style.FramePadding.x * 2.0f);
            if (ImGui::Button(ICON_LC_X))
            {
                filter->active = 0;
            }

            switch (filter->type)
            {
            case FilterType_Path: {
                StringBuilder *buf_p = &filter->val_str.val;
                ImGui::InputTextEx("##Input", "/path/to/file", CStrCast(filter->val_str.val), 512, ImVec2(-1, 0), ImGuiInputTextFlags_CallbackEdit, text_callback, &buf_p);

                ImGui::Text("Exclude");
                ImGui::SameLine();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::Checkbox("##Exclude", (bool *)&filter->val_str.exclude);
                ImGui::PopStyleVar();
            }
            break;
            case FilterType_SizeBetween: {
                ImVec2 right_edge = ImGui::GetContentRegionMax();

                F32 from_align = right_edge.x - SPACING(16) - ImGui::CalcTextSize(CStrCast(byte_string(filter->val_byte.from.unit))).x - ImGui::CalcTextSize(ICON_LC_POWER).x - 4.0f * style.FramePadding.x;
                ImGui::PushStyleColor(ImGuiWindowDockStyleCol_Text, filter->val_byte.from_enable ? MSCBL_FOREGROUND : MSCBL_FOREGROUND_MUTED);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Bigger than ");

                ImGui::SameLine(0, 0);
                ImGui::SetCursorPosX(from_align);
                ImGui::SetNextItemWidth(SPACING(16.0f));
                ImGui::DragFloat("##FromBytes", &filter->val_byte.from.value, 0.1f, 0.0f, 1023.0f, "%.2f");

                ImGui::SameLine(0, 0);
                if (ImGui::BeginCombo("##FromMultiplier", CStrCast(byte_string(filter->val_byte.from.unit)), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_WidthFitPreview))
                {
                    for (U32 i = 0; i < Byte_COUNT; i++)
                    {
                        B32 is_selected = (filter->val_byte.from.unit == (ByteUnit)i);
                        if (ImGui::Selectable(CStrCast(byte_string((ByteUnit)i)), is_selected))
                        {
                            filter->val_byte.from.unit = (ByteUnit)i;
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Button, COLOR_TRANSPARENT);
                if (ImGui::Button(ICON_LC_POWER "##From"))
                {
                    filter->val_byte.from_enable = !filter->val_byte.from_enable;
                }
                ImGui::PopStyleColor(2);

                F32 to_align = right_edge.x - SPACING(16) - ImGui::CalcTextSize(CStrCast(byte_string(filter->val_byte.to.unit))).x - ImGui::CalcTextSize(ICON_LC_POWER).x - 4.0f * style.FramePadding.x;
                ImGui::PushStyleColor(ImGuiWindowDockStyleCol_Text, filter->val_byte.to_enable ? MSCBL_FOREGROUND : MSCBL_FOREGROUND_MUTED);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Smaller than ");

                ImGui::SameLine(0, 0);
                ImGui::SetCursorPosX(to_align);
                ImGui::SetNextItemWidth(SPACING(16.0f));
                ImGui::DragFloat("##ToBytes", &filter->val_byte.to.value, 0.1f, 0.0f, 1023.0f, "%.2f");

                ImGui::SameLine(0, 0);
                if (ImGui::BeginCombo("##ToMultiplier", CStrCast(byte_string(filter->val_byte.to.unit)), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_WidthFitPreview))
                {
                    for (U32 i = 0; i < Byte_COUNT; i++)
                    {
                        B32 is_selected = (filter->val_byte.to.unit == (ByteUnit)i);
                        if (ImGui::Selectable(CStrCast(byte_string((ByteUnit)i)), is_selected))
                        {
                            filter->val_byte.to.unit = (ByteUnit)i;
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Button, COLOR_TRANSPARENT);
                if (ImGui::Button(ICON_LC_POWER "##To"))
                {
                    filter->val_byte.to_enable = !filter->val_byte.to_enable;
                }
                ImGui::PopStyleColor(2);

                if (!filter->val_byte.from_enable && !filter->val_byte.to_enable)
                {
                    filter->active = 0;
                }
            }
            break;
            case FilterType_DateAddedBetween:
            case FilterType_DateCreatedBetween:
            case FilterType_DateModifiedBetween: {
                ImVec2 right_edge = ImGui::GetContentRegionMax();

                F32 right_align = right_edge.x - SPACING(33.0f) - ImGui::CalcTextSize(ICON_LC_POWER).x - 2.0f * style.FramePadding.x;
                ImGui::PushStyleColor(ImGuiWindowDockStyleCol_Text, filter->val_time.from_enable ? MSCBL_FOREGROUND : MSCBL_FOREGROUND_MUTED);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("From");

                ImGui::SameLine(0, 0);
                ImGui::SetCursorPosX(right_align);
                ImGui::SetNextItemWidth(SPACING(8.0f));
                ImGui::DragInt("##DateFrom", (S32 *)&filter->val_time.from.date, 1.0f, 1, month_days(filter->val_time.from), "%02d", ImGuiSliderFlags_WrapAround);

                ImGui::SameLine(0, 0);
                ImGui::SetNextItemWidth(SPACING(12.0f));
                if (ImGui::BeginCombo("##MonthFrom", CStrCast(month_string(filter->val_time.from.month)), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_NoArrowButton))
                {
                    for (U32 i = 0; i < Month_COUNT; i++)
                    {
                        B32 is_selected = (filter->val_time.from.month == (Month)i);
                        if (ImGui::Selectable(CStrCast(month_string((Month)i)), is_selected))
                        {
                            filter->val_time.from.month = (Month)i;
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine(0, 0);
                ImGui::SetNextItemWidth(SPACING(13.0f));
                ImGui::InputInt("##YearFrom", &filter->val_time.from.year, 0, 0);

                ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Button, COLOR_TRANSPARENT);
                if (ImGui::Button(ICON_LC_POWER "##From"))
                {
                    filter->val_time.from_enable = !filter->val_time.from_enable;
                }
                ImGui::PopStyleColor(2);

                ImGui::PushStyleColor(ImGuiWindowDockStyleCol_Text, filter->val_time.to_enable ? MSCBL_FOREGROUND : MSCBL_FOREGROUND_MUTED);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Till");

                ImGui::SameLine(0, 0);
                ImGui::SetCursorPosX(right_align);
                ImGui::SetNextItemWidth(SPACING(8.0f));
                ImGui::DragInt("##DateTo", (S32 *)&filter->val_time.to.date, 1.0f, 1, month_days(filter->val_time.to), "%02d", ImGuiSliderFlags_WrapAround);

                ImGui::SameLine(0, 0);
                ImGui::SetNextItemWidth(SPACING(12.0f));
                if (ImGui::BeginCombo("##MonthTo", CStrCast(month_string(filter->val_time.to.month)), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_NoArrowButton))
                {
                    for (U32 i = 0; i < Month_COUNT; i++)
                    {
                        B32 is_selected = (filter->val_time.to.month == (Month)i);
                        if (ImGui::Selectable(CStrCast(month_string((Month)i)), is_selected))
                        {
                            filter->val_time.to.month = (Month)i;
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine(0, 0);
                ImGui::SetNextItemWidth(SPACING(13.0f));
                ImGui::InputInt("##YearTo", &filter->val_time.to.year, 0, 0);

                ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Button, COLOR_TRANSPARENT);
                if (ImGui::Button(ICON_LC_POWER "##To"))
                {
                    filter->val_time.to_enable = !filter->val_time.to_enable;
                }
                ImGui::PopStyleColor(2);

                if (!filter->val_time.from_enable && !filter->val_time.to_enable)
                {
                    filter->active = 0;
                }
            }
            break;
            case FilterType_EmbeddingDistanceBetween: {
                ImVec2 right_edge = ImGui::GetContentRegionMax();

                F32 right_align = right_edge.x - SPACING(16.0f) - ImGui::CalcTextSize(ICON_LC_POWER).x - 2.0f * style.FramePadding.x;

                ImGui::PushStyleColor(ImGuiWindowDockStyleCol_Text, filter->val_float.from_enable ? MSCBL_FOREGROUND : MSCBL_FOREGROUND_MUTED);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("From");

                ImGui::SameLine(0, 0);
                ImGui::SetCursorPosX(right_align);
                ImGui::SetNextItemWidth(SPACING(16.0f));
                ImGui::DragFloat("##EmbeddingFrom", &filter->val_float.from, 1.0f, 0.0f, 100.0f, "%.1f%%");

                ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Button, COLOR_TRANSPARENT);
                if (ImGui::Button(ICON_LC_POWER "##From"))
                {
                    filter->val_float.from_enable = !filter->val_float.from_enable;
                }
                ImGui::PopStyleColor(2);

                ImGui::PushStyleColor(ImGuiWindowDockStyleCol_Text, filter->val_float.to_enable ? MSCBL_FOREGROUND : MSCBL_FOREGROUND_MUTED);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Till");

                ImGui::SameLine(0, 0);
                ImGui::SetCursorPosX(right_align);
                ImGui::SetNextItemWidth(SPACING(16.0f));
                ImGui::DragFloat("##EmbeddingTo", &filter->val_float.to, 1.0f, 0.0f, 100.0f, "%.1f%%");

                ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Button, COLOR_TRANSPARENT);
                if (ImGui::Button(ICON_LC_POWER "##To"))
                {
                    filter->val_float.to_enable = !filter->val_float.to_enable;
                }
                ImGui::PopStyleColor(2);
            }
            break;
            default: break;
            }

            ImGui::PopStyleVar();
        }
    }
}

void menu_sidebar()
{
    if (ImGui::Button(sidebar_open ? ICON_LC_CHEVRON_LEFT : ICON_LC_CHEVRON_RIGHT, {ImGui::GetContentRegionAvail().x, 0}))
    {
        sidebar_open = !sidebar_open;
        needs_rebuild = 1;
    }

    ImGuiStyle &style = ImGui::GetStyle();
    F32 frame_height = ImGui::GetFrameHeight();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (sidebar_open)
    {
        if (ImGui::Button(ICON_LC_REFRESH_CW "  Rescan Images", ImVec2((avail.x - style.ItemSpacing.x) / 2.0f, 0)))
        {
            // threadpool_enqueue(TaskPriority_High, {.func = cont_scan});
            ui_push_message({.success = 0, .domain = Domain_App, .code = 1, .context = "implementation pending"});
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_LC_PLUS " Add Folders", ImVec2((avail.x - style.ItemSpacing.x) / 2.0f, 0)))
        {
            scan_new_dir(ui_state.arena);
        }
    }
    else
    {
        if (ImGui::Button(ICON_LC_REFRESH_CW, {ImGui::GetContentRegionAvail().x, 0}))
        {
            // threadpool_enqueue(TaskPriority_High, {.func = cont_scan});
            ui_push_message({.success = 0, .domain = Domain_App, .code = 1, .context = "implementation pending"});
        }
        if (ImGui::Button(ICON_LC_PLUS, {ImGui::GetContentRegionAvail().x, 0}))
        {
            scan_new_dir(ui_state.arena);
        }
    }

    if (sidebar_open)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_TRANSPARENT);
        DeferLoop(ImGui::BeginChild("FiltersGroup", ImVec2(avail.x, avail.y * 0.666f), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY), ImGui::EndChild())
        {
            ImGui::PopStyleColor();
            ImGui::PushFont(ui_state.title_font);
            ImGui::Text("Filters");
            ImGui::PopFont();
            if (ImGui::Button(ICON_LC_FUNNEL_PLUS " Add filter", ImVec2((avail.x - style.ItemSpacing.x) / 2.0f, 0)))
            {
                ui_add_filter();
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply", ImVec2((avail.x - style.ItemSpacing.x) / 2.0f, 0)))
            {
                view_fetch_map();
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_TRANSPARENT);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
            DeferLoop(ImGui::BeginChild("Filters"), ImGui::EndChild())
            {
                ImGui::PopStyleColor();
                filters();
            }
            ImGui::PopStyleVar();
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_TRANSPARENT);
        DeferLoop(ImGui::BeginChild("Directories", ImVec2(avail.x, avail.y * 0.333f - style.FramePadding.y), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY), ImGui::EndChild())
        {
            ImGui::PopStyleColor();
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::PushFont(ui_state.title_font);
            ImGui::Text("Directories");
            ImGui::PopFont();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, style.FramePadding.y));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, COLOR_TRANSPARENT);
            directory_tree();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);

            ImGui::SetCursorPosY(avail.y - ImGui::GetFrameHeight());
            if (ImGui::Button("Expand All", ImVec2((avail.x - style.ItemSpacing.x) / 2.0f, 0)))
            {
                sidebar_dirtree_openall = 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Collapse All", ImVec2((avail.x - style.ItemSpacing.x) / 2.0f, 0)))
            {
                sidebar_dirtree_foldall = 1;
            }
        }
    }
}
