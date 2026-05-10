#pragma once
#include "base/threadpool.h"
#include "inference/clip.h"

struct CLIPModel
{

    clip_ctx *clip;
};

MSCBL_API CLIPModel model;
MSCBL_API Arena *model_arena;

ThreadFunc(model_insert_embedding);
Embedding model_embed_text(Arena* arena, String text);
