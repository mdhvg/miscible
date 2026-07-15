#pragma once
#include "base/base_core.h"

#define APP_NAME         Miscible
#define APP_NAME_DISPLAY miscible

#define ATLAS_DIR   "atlas"
#define DB_FILE     "miscible.sqlite"
#define CONFIG_FILE "miscible.yaml"

#define ATLAS_CAPACITY 100
#define ATLAS_SIZE     2560
#define ATLAS_CHANNELS 4
#define THUMB_PER_SIDE 10
#define THUMB_SIZE     256

#if OS_WIN32
#define LOG_BASE_ENV "LOCALAPPDATA"
#define LOG_APPEND   Stringify(APP_NAME) "\\logs\\"
#elif OS_LINUX
#define LOG_BASE_ENV "HOME"
#define LOG_APPEND   ".local/share/" Stringify(APP_NAME) "/logs/"
#endif

#define LOG_FILE_NAME_PRE Stringify(APP_NAME) "_log_"
#define LOG_FILE_NAME_EXT ".log"

enum AppResultDomain
{
    __AppResultDomain_Begin = 1000,
    Domain_YAML,
    Domain_JSON,
    Domain_Inference_ONNX,
    Domain_Inference_GGML,
};

enum AppError
{
    AppError_None = 0,
    AppError_NullPtr,
    AppError_ChecksumFail,
    AppError_FileNotFound,
    AppError_PartialDownload,
    AppError_UnImplemented,
};

MSCBL_API S32 mscbl_start(S32 argc, char **argv);
