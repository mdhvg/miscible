#pragma once
#include "base/base_core.h"
#include "base/string.h"

struct Config
{
    String home_path;
    String atlas_dir;
    String db_path;
};

MSCBL_API Config mscbl_config;

void config_init();
