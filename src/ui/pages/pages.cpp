#include "ui/pages/pages.h"
#include "ui/pages/menu/menu.cpp"
#include "ui/pages/style.cpp"
#include "ui/pages/preview/preview.cpp"

S32 text_callback(ImGuiInputTextCallbackData *data)
{
    StringBuilder **buf = (StringBuilder **)data->UserData;

    if (data->BufTextLen >= (*buf)->size)
    {
        String input = {
            .v = (U8 *)(data->Buf + (*buf)->size),
            .size = (U64)(data->BufTextLen - (*buf)->size)};
        string_push(*buf, input);
    }
    else
    {
        string_pop_to((*buf), data->BufTextLen);
    }

    return 0;
}
