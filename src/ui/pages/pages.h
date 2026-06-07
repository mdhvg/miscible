#pragma once
#include "imgui.h"
#include "imgui_internal.h"

#include "base/base_core.h"

typedef void (*PageFn)(void);

enum UIPage
{
    UIPage_MENU = 0,
    UIPage_PREVIEW,
    UIPage_COUNT
};

#if DBG
struct PageData
{
    const char *fn_name;
    PageFn fn;
};

global_v PageData page_data[] = {
    {"page_menu", NULL},
    {"page_preview", NULL},
};
#else
MSCBL_EXP void restyle();
MSCBL_EXP void page_menu();
MSCBL_EXP void page_preview();

global_v PageFn page_data[] = {
    page_menu,
    page_preview,
};
#endif

local_v B32 switch_page = 0;
local_v B32 needs_rebuild = 1;
local_v ImVec2 last_work_size = {0, 0};
