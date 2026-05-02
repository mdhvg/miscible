#include "base/arena.h"
#include "base/base_core.h"
#include "imgui.h"

void ui_debug_arenas()
{
    ImGui::Begin("Arena Debug");

    for (Arena *cur = arena_head; cur->next; cur = cur->next)
    {
        ImGui::Text("Arena: %s", cur->name);

        SizeUnits used = formatted_size(cur->used);
        SizeUnits cap  = formatted_size(cur->capacity);
        ImGui::Text("Used %.3f%s of %.3f%s", used.size, used.unit, cap.size, cap.unit);
        ImGui::ProgressBar((F64)cur->used / (F64)cur->capacity);
    }

    ImGui::End();
}
