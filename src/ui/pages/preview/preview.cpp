#include "imgui.h"
#include "ui/pages/pages.h"
#include "ui/pages/preview/preview.h"

#include "config.h"
#include "db/fetch.h"
#include "ui/theme.h"
#include "ui/ui_core.h"

#define ZOOM_SPEED 0.1f
#define MIN_ZOOM   1.0f
#define MAX_ZOOM   10.0f

local_v Image *image = NULL;
local_v B32 infobar_open = 0;
local_v F32 canvas_zoom = 1.0f;
local_v ImVec2 canvas_offset = {0.0f, 0.0f};

void image_canvas()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 0.0f);
    ImVec2 avl = ImGui::GetContentRegionAvail();
    if (avl.x <= 0.0f || avl.y <= 0.0f)
        return;

    F32 aspect = (F32)image->width / (F32)image->height;

    F32 base_w = 0,
        base_h = 0,
        zoomed_w = 0,
        zoomed_h = 0;

    base_w = MIN((F32)image->width, avl.x);
    base_h = base_w / aspect;
    base_h = MIN(base_h, avl.y);
    base_w = base_h * aspect;

    zoomed_w = base_w * canvas_zoom;
    zoomed_h = base_h * canvas_zoom;

    if (canvas_zoom <= 1.0f)
    {
        canvas_zoom = 1.0f;
        canvas_offset = {0.0f, 0.0f};
    }

    ImVec2 start_pos;
    start_pos.x = ((avl.x - zoomed_w) / 2.0f) + canvas_offset.x;
    start_pos.y = ((avl.y - zoomed_h) / 2.0f) + canvas_offset.y;

    ImVec2 mouse_pos = ImGui::GetMousePos();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    ImVec2 center_pos = ImVec2(avl.x / 2.0f, avl.y / 2.0f);

    ImVec2 mouse_local = ImVec2(mouse_pos.x - cursor_pos.x, mouse_pos.y - cursor_pos.y);
    ImVec2 pointed_loc = ImVec2((center_pos.x - mouse_local.x) * (canvas_zoom - 1.0f), (center_pos.y - mouse_local.y) * (canvas_zoom - 1.0f));

    start_pos.x += pointed_loc.x;
    start_pos.y += pointed_loc.y;

    ImGui::SetCursorPos(start_pos);
    ImGui::Image((ImTextureRef)ui_preview.texture, ImVec2(zoomed_w, zoomed_h), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));

    // TODO: check pending from here on
    B32 is_hovered = ImGui::IsItemHovered();
    B32 is_active = ImGui::IsItemActive();

    if (is_hovered && ImGui::GetIO().MouseWheel != 0.0f)
    {
        F32 old_zoom = canvas_zoom;
        canvas_zoom += ImGui::GetIO().MouseWheel * ZOOM_SPEED;
        canvas_zoom = ImClamp(canvas_zoom, MIN_ZOOM, MAX_ZOOM);

        if (canvas_zoom > 1.0f)
        {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            F32 zoom_ratio = canvas_zoom / old_zoom;
        }
    }

    if (is_active && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Left)))
    {
        ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
        canvas_offset.x += mouse_delta.x;
        canvas_offset.y += mouse_delta.y;
    }

    ImGui::PopStyleVar();
}

void preview_draw_docked_main(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));
    ImGui::Begin("MainPanel", NULL, flags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (ImGui::Button(ICON_LC_MOVE_LEFT))
    {
        switch_page = 1;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_TRANSPARENT);
    ImGui::BeginChild("Canvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    image_canvas();

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void preview_draw_docked_infobar(ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MSCBL_OUTER_PADDING, MSCBL_OUTER_PADDING));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::Begin("InfobarPanel", NULL, flags);

    if (ImGui::Button(ICON_LC_X))
    {
        infobar_open = 0;
        needs_rebuild = 1;
    }

    ImGui::Text("%d x %d x %d", image->width, image->height, image->channels);
    ImGui::Text("%.2f %s", image->size.value, CStrCast(byte_string(image->size.unit)));

    if (ImGui::IsItemClicked())
    {
        // TODO: Open image in native file explorer
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

MSCBL_EXP void page_preview()
{
    switch_page = 0;
    if (!image)
    {
        image = &images[ui_preview.image_id];
    }

    if (!image->width || !image->height)
    {
    }

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
    {
    }

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        switch_page = 1;
    }

    if (ImGui::IsKeyChordPressed(ImGuiKey_Enter | ImGuiMod_Alt))
    {
        infobar_open = !infobar_open;
        needs_rebuild = 1;
    }

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (last_work_size.x != viewport->WorkSize.x || last_work_size.y != viewport->WorkSize.y)
        needs_rebuild = 1;
    last_work_size = viewport->WorkSize;

    F32 infobar_width = infobar_open ? SPACING(infobar_open_units) : SPACING(infobar_fold_units);

    if (ImGui::DockBuilderGetNode(dockspace_id) == NULL || needs_rebuild)
    {
        needs_rebuild = 0;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id);
        ImGui::DockBuilderSetNodeSize(dockspace_id, last_work_size);

        ImGuiID dock_main_id = dockspace_id;

        if (infobar_open)
        {
            F32 infobar_ratio = infobar_width / last_work_size.x;
            ImGuiID dock_id_infobar = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, infobar_ratio, NULL, &dock_main_id);

            ImGui::DockBuilderDockWindow("InfobarPanel", dock_id_infobar);
        }

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

    preview_draw_docked_main(window_flags);

    if (infobar_open)
    {
        preview_draw_docked_infobar(window_flags);
    }

    if (switch_page)
    {
        canvas_offset = ImVec2(0.0f, 0.0f);
        canvas_zoom = 0.0f;
        image = NULL;

        needs_rebuild = 1;
        ui_state.page = UIPage_MENU;
    }
}
