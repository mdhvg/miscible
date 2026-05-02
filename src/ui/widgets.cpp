#include "imgui.h"
#include "db/view.h"
#include "ui/widgets.h"

global_v const char *byte_string(ByteUnit unit)
{
    switch (unit)
    {
    case Byte: return "B";
    case KiByte: return "KB";
    case MiByte: return "MB";
    case GiByte: return "GB";
    case TiByte: return "TB";
    case PiByte: return "PB";
    default:
        Assert(unit == -1, "what's this?");
        return NULL;
    }
}

global_v const char *month_string(Month month)
{
    switch (month)
    {
    case Jan: return "JAN";
    case Feb: return "FEB";
    case Mar: return "MAR";
    case Apr: return "APR";
    case May: return "MAY";
    case Jun: return "JUN";
    case Jul: return "JUL";
    case Aug: return "AUG";
    case Sep: return "SEP";
    case Oct: return "OCT";
    case Nov: return "NOV";
    case Dec: return "DEC";
    default:
        Assert(month == -1, "what's this?");
        return NULL;
    }
}

void input_bytesize(ByteSize *source)
{
    ImGui::InputFloat("##Bytes", &source->value, 0.1, 0.5, "%.2f", ImGuiInputTextFlags_EscapeClearsAll);

    ImGui::SameLine();

    if (ImGui::BeginCombo("##Multiplier", byte_string(source->unit), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
    {
        for (U32 i = 0; i < ByteUnit_COUNT; i++)
        {
            if (ImGui::Selectable(byte_string((ByteUnit)i), 1))
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
    case Jan:
    case Mar:
    case May:
    case Jul:
    case Aug:
    case Oct:
    case Dec:
        end_date = 31;
        break;

    case Apr:
    case Jun:
    case Sep:
    case Nov:
        end_date = 30;
        break;

    case Feb:
        if (source->year % 4 == 0 && (source->year % 100 != 0 || source->year % 400 == 0))
            end_date = 29;
        else
            end_date = 28;
        break;
    default: break;
    }
    ImGui::DragInt("##Date", &source->date, 1.0f, 1, end_date, "%02d", ImGuiSliderFlags_WrapAround);

    ImGui::SameLine();

    if (ImGui::BeginCombo("##Month", month_string(source->month), ImGuiComboFlags_HeightSmall | ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_NoArrowButton))
    {
        for (U32 i = 0; i < Month_COUNT; i++)
        {
            if (ImGui::Selectable(month_string((Month)i), 1))
                source->month = (Month)i;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    ImGui::DragInt("##Year", &source->year, 1.0f, 1900);
}
