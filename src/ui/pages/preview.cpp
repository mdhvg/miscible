#include "IconsMaterialSymbols.h"
#include "base/array.h"
#include "gl/gl_core.h"
#include "window/window.h"
#include "db/fetch.h"
#include "imgui.h"

#include "ui/components/button.h"
#include "base/string.h"
#include "ui/ui_core.h"
#include "ui/ui_utils.h"

#define PREVIEW_CACHE 10

struct PreviewState
{
    B32 infobar_open;
    Image *cur;
    // U32 texture_cache[PREVIEW_CACHE];
    U32 texture;
};

PreviewState preview_state = {0};

global_v const F32 infobar_open_w = SPACING(80);

void image_canvas()
{
    F32 w, h;
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImVec2 avl   = ImGui::GetContentRegionAvail();
    F32 aspect   = (float)preview_state.cur->width / (float)preview_state.cur->height;

    w = MIN((float)preview_state.cur->width, avl.x);
    h = w / aspect;
    h = MIN(h, avl.y);
    w = h * aspect;

    start.x = (avl.x - w) / 2.0f;
    start.y = (avl.y - h) / 2.0f;

    ImGui::SetCursorPos(start);
    ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 1);

    ImGui::Image((ImTextureRef)preview_state.texture, {w, h}, {0, 0}, {1, 1});

    ImGui::PopStyleVar();
}

void image_view()
{
    preview_state.infobar_open
        ? ImGui::BeginChild("Image", {(F32)win.width - infobar_open_w, (F32)win.height})
        : ImGui::BeginChild("Image", {(F32)win.width, (F32)win.height});

    BUTTON_GHOST_START;
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {1, 0.5});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {SPACING(1), SPACING(1)});
    if (ImGui::Button(ICON_MS_ARROW_BACK))
        ui_state.page = UIPage_MENU;
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    BUTTON_GHOST_END;

    image_canvas();

    ImGui::EndChild();
}

void image_infobar()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, DARK_SIDEBAR);
    ImGui::BeginChild("Infobar", {infobar_open_w, (F32)win.height});

    ImVec2 pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos({pos.x + SPACING(1), pos.y + SPACING(1)});

    BUTTON_GHOST_START;
    if (ImGui::Button(ICON_MS_CLOSE))
        preview_state.infobar_open = 0;
    BUTTON_GHOST_END;

    pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos({pos.x + SPACING(2), pos.y + SPACING(4)});

    ImVec2 info_size = ImGui::GetContentRegionAvail();
    info_size.x -= SPACING(4);
    info_size.y -= SPACING(5);

    ImGui::BeginChild("Info", info_size);

    Image *img = preview_state.cur;

    ImGui::Text("%d x %d x %d", img->width, img->height, img->channels);

    SizeUnits fmt_size = formatted_size(img->size);
    ImGui::Text("%.2f %s", fmt_size.size, fmt_size.unit);

    ImGui::PushStyleColor(ImGuiCol_Text, DARK_PRIMARY);
    ImGui::TextWrapped(CStrCast(img->path));
    if (ImGui::IsItemClicked())
    {
        // TODO: Open the image in file explorer
    }
    ImGui::PopStyleColor();

    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

MSCBL_EXP void ui_preview()
{
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
    {
        // ui_state.active = MAX(0, ui_state.active - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        // ui_state.active = MIN((S64)ui_state.active + 1, da_getsize(ui_state.images) - 1);
    }

    // preview_state.cur = &ui_state.images[ui_state.active];
    if (preview_state.cur->width == 0 || preview_state.cur->height == 0)
    {
        // get_info(preview_state.cur->id, preview_state.cur);
        // gl_make_texture(&preview_state.texture, preview_state.cur->path);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        ui_state.page = UIPage_MENU;
    }

    if (ImGui::IsKeyChordPressed(ImGuiKey_Enter | ImGuiMod_Alt))
    {
        preview_state.infobar_open = !preview_state.infobar_open;
    }

    image_view();
    ImGui::SameLine();
    if (preview_state.infobar_open)
        image_infobar();
}
