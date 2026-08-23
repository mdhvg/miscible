// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "os/os_inc.h"
#include "base/arena.h"
#include "ui/ui_core.h"
#include "ui/ui_utils.h"

#if DBG
global_v LibHandle pages = 0;
global_v PageFn restyle = NULL;

void ui_reload()
{
    if (pages)
        os_closelib(pages);
    pages = os_loadlib("build/pages");
    restyle = (PageFn)os_libfunc(pages, "restyle");

    for (S32 i = 0; i < UIPage_COUNT; i++)
    {
        page_data[i].fn = (PageFn)os_libfunc(pages, page_data[i].fn_name);
    }
}
#endif
