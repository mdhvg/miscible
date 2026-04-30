#pragma once
#include "base/base_core.h"

typedef void (*UIfn)(void);

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
    UIfn fn;
};

global_v PageData page_data[] = {
    {"ui_menu", NULL},
    {"ui_preview", NULL},
};
#else
MSCBL_EXP void restyle();
MSCBL_EXP void ui_menu();
MSCBL_EXP void ui_preview();

global_v UIfn page_data[] = {
    ui_menu,
    ui_preview};
#endif
