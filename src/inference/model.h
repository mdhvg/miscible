#pragma once
#include <queue>

#include "Inference/clip.h"
#include "base/core.h"

struct ImageEmbedding
{
	unsigned int id;
	fs::path path;
	float *embedding;
};

clip_ctx *clip_init();
clip_ctx *clip_get();

std::queue<ImageEmbedding> find_pending_embeddings();
void clip_process_images(std::queue<ImageEmbedding> *image_id_path);