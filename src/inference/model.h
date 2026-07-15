#pragma once
#include "config.h"
#include "base/threadpool.h"
#include "inference/clip.h"

struct CLIPModel
{
    clip_ctx *clip;
};

MSCBL_API CLIPModel model;

ThreadFunc(model_insert_embedding);
Embedding model_embed_text(Arena *arena, String text);
Result model_download_files(Arena *arena, RemoteFileArr files, String model_base);
// B32 model_download(Arena *arena, GGMLConfig *model);
