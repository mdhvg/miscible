#pragma once
#include "sha2.h"
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

struct ModelConfig
{
    String path;
    String filename;
    U8 model_hash[SHA512_DIGEST_SIZE];
    U8 manifest_hash[SHA512_DIGEST_SIZE];
    String *model_url;
    String *manifest_url;
};

struct ModelGroupConfig
{
    String base_dir;
    ModelConfig clip_model;
};

struct Config
{
    String app_data;
    String atlas_dir;
    String db_path;

    Settings settings;
    ViewSettings view_settings;

    ModelGroupConfig model_group;
};

MSCBL_API Config mscbl_config;

void config_init();
