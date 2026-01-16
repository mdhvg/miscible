#pragma once
#include "base/base_core.h"
#include "ui/ui_core.h"
#include "inference/clip.h"

extern struct ScanState scan_state;

global_v Arena *scan_scratch = NULL;
global_v Arena *scan_arena   = NULL;

// Visible functions
void scan_restart();
void scan_update();

// Dir scanning
THREAD_FUNC(scan_dirs);
local_v void scan_after_dir();

// Atlas building
local_v U64 scan_atlas_count();
local_v void scan_make_atlas();
local_v void scan_read_images();
local_v void scan_after_atlas();

// Atlas loading
local_v U64 scan_load_atlas_count();
local_v void scan_load_atlas();
local_v void scan_load_atlas_after();

// UI reload
// TODO: Bring that here

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

local_v U64 scan_embed_count();
local_v void scan_embed_batch();
