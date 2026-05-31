#include "os/os_inc.h"
#include "base/arena.h"
#include "ui/ui_core.h"
#include "ui/ui_utils.h"
#include "ui/pages/menu/menu.h"

Arena *ui_arena = NULL;

const char *byte_size[] = {"B", "KB", "MB", "GB", "PB"};

SizeUnits formatted_size(U64 size)
{
    U64 size_int = size;
    F32 size_float = 0;
    U8 unit_ptr = 0;
    while (size_int >= 1024)
    {
        size_float = (size_int % 1024) / 1000.0f;
        size_int /= 1024;
        unit_ptr += 1;
    }
    F32 final_size = (float)size_int + size_float;
    return {final_size, byte_size[unit_ptr]};
}

#if DBG
global_v LibHandle pages = 0;
global_v UIfn restyle = NULL;

void ui_reload()
{
    if (pages)
        os_closelib(pages);
    pages = os_loadlib("build/pages");
    restyle = (UIfn)os_libfunc(pages, "restyle");

    for (S32 i = 0; i < UIPage_COUNT; i++)
    {
        page_data[i].fn = (UIfn)os_libfunc(pages, page_data[i].fn_name);
    }
    arena_clear(ui_state.arena);
}
#endif
