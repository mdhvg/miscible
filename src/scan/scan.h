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
MSCBL_API void scan_new_dir(Arena *arena);
void scan_atlas_bake(Arena *arena, ImageRow *inserted);

// Atlas building
// local_v U64 scan_atlas_count();
// local_v void scan_make_atlas();
// local_v void scan_read_images();
// local_v void scan_after_atlas();

// Atlas loading
// local_v U64 scan_load_atlas_count();
// local_v void scan_load_atlas();
// local_v void scan_load_atlas_after();

// Embed images
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

// local_v U64 scan_embed_count();
// local_v void scan_embed_batch();
