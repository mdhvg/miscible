// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

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
    {.fn_name = "page_menu", .fn = NULL},
    {.fn_name = "page_preview", .fn = NULL},
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

S32 text_callback(ImGuiInputTextCallbackData *data);
