// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "ggml.h"
#include "ggml-alloc.h"

#include "base/string.h"
#include "db/db_helpers.h"
#include "base/threadpool.h"

extern Arena *scan_arena;

struct ImageRow
{
    S64 id;
    String path;
    U8 *data;
    S32 width;
    S32 height;
    S32 channels;
    S32 resize_width;
    S32 resize_height;
};

// Dir scanning
void cont_scan(Arena *arena);
void first_scan(Arena *arena, OSString dir);
ThreadFunc(read_image);
ThreadFunc(draw_image);
ThreadFunc(scan_routine);
DBStmtCbk(push_imagerow);
void scan_atlas_bake(Arena *arena, ImageRow *inserted);

MSCBL_API inline void scan_directory()
{
    AsyncTask task = {.func = scan_routine};
    threadpool_enqueue(TaskPriority_High, task);
}

enum VisionWorkerStatus
{
    Vision_None,
    Vision_GraphInit,
    Vision_AllocatorInit
};

struct VisionWorker
{
    ggml_context *ctx;
    ggml_cgraph *graph;
    ggml_gallocr_t allocr;
    VisionWorkerStatus valid;
};
