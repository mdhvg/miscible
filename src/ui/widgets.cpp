#include "base/base_core.h"
#include "base/string.h"
#include "imgui.h"
#include "db/view.h"
#include "ui/widgets.h"

void input_bytesize(ByteSize *source)
{
    ImGui::InputFloat("##Bytes", &source->value, 0.1, 0.5, "%.2f", ImGuiInputTextFlags_EscapeClearsAll);

    ImGui::SameLine();

    if (ImGui::BeginCombo("##Multiplier", CStrCast(byte_string(source->unit)), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
    {
        for (U32 i = 0; i <= PiByte; i++)
        {
            if (ImGui::Selectable(CStrCast(byte_string((ByteUnit)i)), 1))
                source->unit = (ByteUnit)i;
        }
        ImGui::EndCombo();
    }
}

void input_date(Date *source)
{
    S32 end_date = 31;
    switch (source->month)
    {
    case Month_Jan:
    case Month_Mar:
    case Month_May:
    case Month_Jul:
    case Month_Aug:
    case Month_Oct:
    case Month_Dec:
        end_date = 31;
        break;

    case Month_Apr:
    case Month_Jun:
    case Month_Sep:
    case Month_Nov:
        end_date = 30;
        break;

    case Month_Feb:
        if (source->year % 4 == 0 && (source->year % 100 != 0 || source->year % 400 == 0))
            end_date = 29;
        else
            end_date = 28;
        break;
    default: break;
    }
    ImGui::DragInt("##Date", &source->date, 1.0f, 1, end_date, "%02d", ImGuiSliderFlags_WrapAround);

    ImGui::SameLine();

    if (ImGui::BeginCombo("##Month", CStrCast(month_string(source->month)), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
    {
        for (U32 i = 0; i <= Month_Dec; i++)
        {
            if (ImGui::Selectable(CStrCast(month_string((Month)i)), 1))
                source->month = (Month)i;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    ImGui::DragInt("##Year", &source->year, 1.0f, 1900);
}
