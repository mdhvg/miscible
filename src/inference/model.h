#pragma once
#include "base/threadpool.h"
#include "inference/clip.h"

struct CLIPModel
{
    clip_ctx *clip;
};

MSCBL_API CLIPModel model;
MSCBL_API Arena *model_arena;

struct Embedding
{
    F32 *vector;
    S32 size;
};

Embedding embed_text(String text);
