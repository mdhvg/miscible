#pragma once
#include "db/view.h"

struct Settings
{
    U64 scan_depth;
    F32 font_size;
};

struct ViewSettings
{
    SortType sort_basis;
    B32 descending;
};

struct Config
{
    String app_data;
    String atlas_dir;
    String db_path;

    Settings settings;
    ViewSettings view_settings;
};

MSCBL_API Config mscbl_config;

void config_init();
