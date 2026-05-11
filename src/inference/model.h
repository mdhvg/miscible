#pragma once
#include "base/threadpool.h"
#include "inference/clip.h"

struct CLIPModel
{

    clip_ctx *clip;
};

MSCBL_API CLIPModel model;

ThreadFunc(model_insert_embedding);
Embedding model_embed_text(Arena *arena, String text);
