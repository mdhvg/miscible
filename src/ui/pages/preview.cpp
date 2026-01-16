#include "imgui.h"

#include "ui/pages/preview.h"
#include "base/string.h"
#include "ui/ui_core.h"

void ui_preview()
{
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        ui_state.page = UIPage_MENU;
    }
    ImGui::Text(format_cstr(&strbuf, "Preview of %zu", ui_state.active));
}
