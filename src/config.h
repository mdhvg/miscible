// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "sha2.h"
#include "db/view.h"

typedef struct
{
    U64 scan_depth;
    F32 font_size;
    U64 log_age_days;
} Settings;

typedef struct
{
    SortType sort_basis;
    B32 descending;
} ViewSettings;

typedef enum
{
    Backend_ONNX,
    Backend_GGML,
} BackendType;

typedef enum
{
    PREC_UNKNOWN = 0,
    PREC_FP32,
    PREC_FP16,
    PREC_INT8,
    PREC_UINT8,
    PREC_Q4,
    PREC_Q4F16,
    PREC_BNB4
} Precision;

typedef struct
{
    U64 size;
    String url;
    String name;
    U8 hash[32];
} RemoteFile;

typedef struct
{
    int precision;
    const RemoteFile *text;
    U64 text_size;
    const RemoteFile *vision;
    U64 vision_size;
} ModelVariant;

typedef struct
{
    String name;
    const RemoteFile *common_files;
    U64 common_files_size;
    const ModelVariant *variants;
    U64 variants_size;
} ModelGroup;

typedef struct
{
    const ModelGroup *group;
    const ModelVariant *variant;
    BackendType backend;
} ActiveModel;

typedef struct
{
#if OS_WIN32
    U32 dml_device;
#elif OS_LINUX
#endif

    String base_dir;
    ActiveModel active;
} InferenceSettings;

typedef struct
{
    String app_data;
    String atlas_dir;
    String db_path;

    Settings settings;
    ViewSettings view_settings;

    InferenceSettings inf_settings;
} Config;

MSCBL_API Config mscbl_config;
MSCBL_API B32 config_dirty;
#define config_set_var(k, v) ((mscbl_config.k = (v)), config_dirty = 1);

void config_init(Arena *arena);
void config_deinit(Arena *arena);
