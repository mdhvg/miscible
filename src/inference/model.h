#pragma once

#define GGML_DEBUG 100
#include "ggml-alloc.h"
#include "ggml.h"

#include "base/base_core.h"

#define MODEL_BATCH_SIZE 8

struct VisionWorker
{
	ggml_context *ctx;
	ggml_cgraph *graph;
	ggml_gallocr_t allocr;
	U8 valid;
};

void model_init();
void model_create_embeddings();
void model_after_create_embedding();

// clip_ctx *clip_init();
// clip_ctx *clip_get();

// std::queue<ImageEmbedding> find_pending_embeddings();
// void clip_process_images(std::queue<ImageEmbedding> *image_id_path);

// Embedding logic (parallelized)
// 1247
// 1247/8 = 155

// job_count = 155

// job_fn(n) {
// 	base = n * BATCH_SIZE;
// 	end = MIN(base + BATCH_SIZE, load_count);
// 	load_from_to(base, end);
// 	do embed...
// 	db_run(BEGIN TRANSACTION)
// 	db_run(put embedding)
// 	db_run(COMMIT)
// }