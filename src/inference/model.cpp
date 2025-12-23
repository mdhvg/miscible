#include "base/array.h"
#include "base/threadpool.h"
#include "clip.h"
#include "db/db_helpers.h"
#include "os/os_inc.h"

#include "inference/clip.h"
#include "inference/model.h"
#include "inference/preprocess.h"

#include "inference/clip.cpp"
#include "inference/preprocess.cpp"
#include "sqlite3.h"

internal Arena *model_arena		= NULL;
internal ArenaArray embed_arena = {0};

struct ImageEmbedding
{
	U64 id;
	Path path;
};

struct ClipData
{
	clip_ctx clip;

	ggml_context *text_ctx;
	ggml_cgraph *text_graph = NULL;

	DynamicArray(ImageEmbedding, embed_pending_images);
	DynamicArray(VisionWorker, vision_workers);
};

internal ClipData clip_data = {0};

local U64 embed_start = 0;
local U64 embed_end	  = 0;

DB_CALLBACK(find_embeddable)
{
	ImageEmbedding v = {strtoull(argv[0], NULL, 0), s_cpy(model_arena, argv[1])};
	dyn_array_push(model_arena, clip_data.embed_pending_images, v);
	return 0;
}

F32 *preprocess_batch_from_to(Arena *arena, ImageEmbedding *entries, U64 size, U64 from, U64 to)
{
	F32 *data;
	S32 w, h;

	U32 frame_size	= size * size;
	U32 image_size	= frame_size * 3;
	F32 *resized	= push_array(arena, image_size, F32);
	F32 *batch_data = push_array(arena, (image_size * MODEL_BATCH_SIZE), F32);
	F32 *mean		= clip_data.clip.image_mean;
	F32 inv_std[3]	= {
		 1 / clip_data.clip.image_std[0],
		 1 / clip_data.clip.image_std[1],
		 1 / clip_data.clip.image_std[2],
	 };

	U8 batch_idx = 0;
	for (U64 idx = from; idx < to; idx++)
	{
		data = stbi_loadf(str_to_cstr(entries[idx].path), &w, &h, NULL, 3);
		Assert(data);
		stbir_resize_float_linear(data, w, h, 0, resized, size, size, 0, STBIR_RGB);

		// TODO: Please find something to speed it up. This abomination takes ~4s per image to process
		for (U32 i = 0; i < frame_size; i++)
		{
			batch_data[batch_idx * image_size + 0 * frame_size + i] = ((resized[3 * i + 0] - mean[0]) * inv_std[0]); /* red */
			batch_data[batch_idx * image_size + 1 * frame_size + i] = ((resized[3 * i + 1] - mean[1]) * inv_std[1]); /* green */
			batch_data[batch_idx * image_size + 2 * frame_size + i] = ((resized[3 * i + 2] - mean[2]) * inv_std[2]); /* blue */
		}

		stbi_image_free(data);
		batch_idx++;
	}
	if (batch_idx < MODEL_BATCH_SIZE) MemoryZero(batch_data + (batch_idx * image_size), (MODEL_BATCH_SIZE - batch_idx) * image_size * sizeof(F32));

	return batch_data;
}

THREAD_FUNC(embed_batch)
{
	printf("Arena usage: %zu bytes (%.4f)\n", arena->used, (float)(arena->used) / (arena->capacity));

	ggml_context **ctx	= &dyn_array_at(clip_data.vision_workers, id).ctx;
	ggml_cgraph **graph = &dyn_array_at(clip_data.vision_workers, id).graph;
	U8 *valid			= &dyn_array_at(clip_data.vision_workers, id).valid;

	if ((*valid & 0x0F) != 0x0F)
	{
		U64 mem_size = (ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead());
		*ctx		 = ggml_init({mem_size,
								  arena_push(arena, mem_size, GGML_MEM_ALIGN),
								  true});
		*graph		 = build_image_encode_graph(*ctx, &clip_data.clip, MODEL_BATCH_SIZE);
		printf("Graph context mem usage: %zu/%zu (%.6f)\n", ggml_used_mem(*ctx), ggml_get_mem_size(*ctx), (float)ggml_used_mem(*ctx) / (float)ggml_get_mem_size(*ctx));
		*valid |= 0x0F;
	}

	Temp scratch = temp_begin(arena);

	U64 base		= MODEL_BATCH_SIZE * n;
	U64 end			= MIN(base + MODEL_BATCH_SIZE, clip_data.embed_pending_images.size);
	U64 size		= clip_get_vision_hparams(&clip_data.clip)->image_size;
	F32 *image_data = preprocess_batch_from_to(arena, clip_data.embed_pending_images.v, size, base, end);
	// stbi_write_hdr("1.hdr", 224, 224 * 3 * MODEL_BATCH_SIZE, 1, image_data);

	// Create embeddings
	// ggml_init_params graph_params = {graph_size, NULL, true};
	// ggml_context *vision_ctx	  = ggml_init(graph_params);
	// ggml_cgraph *vision_graph	  = build_image_encode_graph(vision_ctx, &clip_data.clip, (end - base));
	F32 *embeddings	   = clip_get_image_embedding(arena, &clip_data.clip, &dyn_array_at(clip_data.vision_workers, id), image_data, MODEL_BATCH_SIZE);
	S32 embedding_size = clip_get_vision_hparams(&clip_data.clip)->projection_dim;

	sqlite3_stmt *stmt = db_prepare("UPDATE Images SET embedding = ? WHERE id = ?;");
	for (U64 i = 0; i < (end - base); i++)
	{
		sqlite3_bind_blob(stmt, 1, embeddings + i * embedding_size, embedding_size * sizeof(F32), SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 2, dyn_array_at(clip_data.embed_pending_images, base + i).id);
		db_run_stmt(stmt);
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	}
	sqlite3_finalize(stmt);
	temp_end(scratch);
}

void model_init()
{
	if (!model_arena) model_arena = arena_alloc(MB(1));
	clip_model_load(model_arena, &clip_data.clip, ROOT_DIR "/CLIP-ViT-B-32-laion2B-s34B-b79K_ggml-model-f16.gguf");

	//db_run("SELECT usearch_create('images_embedding_idx', 'Images', 'embedding', 512, 'cosine');");

	U64 count;
	db_run("SELECT COUNT(id) FROM Images WHERE embedding IS NULL;", get_count, &count);

	clip_data.embed_pending_images = dyn_array_init(model_arena, count, ImageEmbedding);
	db_run("SELECT id, path FROM Images WHERE embedding IS NULL;", find_embeddable);
	printf("Can embed %zu images\n", count);
}

void model_create_embeddings()
{
	if (!embed_arena.size)
		embed_arena = arena_array_alloc(MB(6), os_info.worker_count);
	arena_array_clear(embed_arena);
	embed_start = clock();

	U64 worker_count = MIN(os_info.pool->worker_count, ToCeilInt(clip_data.embed_pending_images.size, MODEL_BATCH_SIZE));

	if (!clip_data.vision_workers.size)
	{
		clip_data.vision_workers = dyn_array_init(model_arena, worker_count, VisionWorker);
		for (U64 i = 0; i < worker_count; i++)
		{
			dyn_array_at(clip_data.vision_workers, i) = {0};
		}
	}

	parallel_for(os_info.pool, worker_count, embed_batch, NULL, &embed_arena);
}

void model_after_create_embedding()
{
	embed_end = clock();
	printf("Embedding time\n\tTotal: %.6f (%zu images)\n\tPer Image (Avg): %.6f\n", ((float)(embed_end - embed_start)) / CLOCKS_PER_SEC, clip_data.embed_pending_images.size, ((float)(embed_end - embed_start)) / (CLOCKS_PER_SEC * clip_data.embed_pending_images.size));
}

// void clip_process_images(std::queue<ImageEmbedding> *image_id_path)
// {
// 	while (!image_id_path->empty())
// 	{
// 		int batch_size = MIN(image_id_path->size(), 4);
// 		float *batch = make_batch(image_id_path, batch_size, 224);

// 		const size_t graph_size = ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE +
// 								  ggml_graph_overhead();
// 		ggml_init_params graph_params = {graph_size, nullptr, true};
// 		vision_ctx = ggml_init(graph_params);
// 		vision_graph = build_image_encode_graph(vision_ctx, clip, batch_size);
// 		float *embeddings = clip_get_image_embedding(clip, vision_ctx, vision_graph,
// 													 batch, batch_size);
// 		SPDLOG_INFO("Made embeddings!");

// 		delete[] batch;
// 		delete[] embeddings;
// 	}
// }

// clip_ctx *clip_get()
// {
// 	return clip;
// }
