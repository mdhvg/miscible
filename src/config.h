#pragma once
#include "base/base_core.h"
#include "base/string.h"

struct Settings
{
    U64 scan_depth;
    F32 font_size;
};

struct Config
{
    String home_path;
    String atlas_dir;
    String db_path;

    Settings settings;
};

MSCBL_API Config mscbl_config;

void config_init();
