// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "sha2.h"
#include "db/view.h"

struct Settings
{
    U64 scan_depth;
    F32 font_size;
    U64 log_age_days;
};

struct ViewSettings
{
    SortType sort_basis;
    B32 descending;
};

struct RemoteFile
{
    U64 size;
    String url;
    String name;
    U8 hash[SHA256_DIGEST_SIZE];
};
typedef RemoteFile *RemoteFileArr;

enum Precision
{
    PREC_UNKNOWN = 0,
    PREC_FP32,
    PREC_FP16,
    PREC_INT8,
    PREC_UINT8,
    PREC_Q4,
    PREC_Q4F16,
    PREC_BNB4
};

struct ModelVariant
{
    Precision precision;
    RemoteFileArr text_files;
    RemoteFileArr vision_files;
};
typedef ModelVariant *ModelVariantArr;

struct ModelGroup
{
    String name;
    RemoteFileArr common_files;
    ModelVariantArr variants;
};
typedef ModelGroup *ModelGroupArr;

enum BackendType
{
    Backend_ONNX,
    Backend_GGML,
};

struct ActiveModel
{
    ModelGroup *group;
    ModelVariant *variant;
    BackendType backend;
};

struct InferenceSettings
{
    String base_dir;
    ActiveModel active;
    ModelGroupArr ggml;
    ModelGroupArr onnx;
};

struct Config
{
    String app_data;
    String atlas_dir;
    String db_path;

    Settings settings;
    ViewSettings view_settings;

    InferenceSettings inf_settings;
};

MSCBL_API Config mscbl_config;

void config_init();
