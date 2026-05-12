#include "base/base_core.h"
#include "ggml.h"
#include "os/os_inc.h"
#include "sqlite3.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

#include "base/arena.h"
#include "base/array.h"
#include "base/log.h"
#include "base/threadpool.h"
#include "db/db_helpers.h"
#include "inference/clip.h"
#include "scan/scan.h"
#include "inference/model.h"

CLIPModel model = {0};
Arena *model_arena = NULL;

#define MODEL_PATH       "./CLIP-ViT-B-32-laion2B-s34B-b79K.gguf"
#define EMBED_BATCH_SIZE 8

struct preprocess_params
{
    String path;
    F32 *write_base;
};

ThreadFunc(preprocess_image)
{
    Assert(data.kind == TPData_ANY, "wrong datatype");
    preprocess_params *params = (preprocess_params *)data.val_any;
    String image_path = params->path;
    F32 *write_base = params->write_base;

    Temp scratch = temp_begin(arena);

    clip_vision_model *vision_model = &model.clip->vision_model;
    clip_vision_hparams hparams = vision_model->hparams;
    S32 image_size = hparams.image_size;
    U32 frame_size = image_size * image_size;
    F32 *resized = push_array(scratch.arena, frame_size * 3, F32);

    S32 w, h;
    F32 *image_data = stbi_loadf(CStrCast(image_path), &w, &h, NULL, 3);
    Assert(image_data, "image data is NULL (%.*s)", StringSpr(image_path));
    stbir_resize_float_linear(image_data, w, h, 0, resized, image_size, image_size, 0, STBIR_RGB);
    stbi_image_free(image_data);

    F32 *r_plane = write_base + (0 * frame_size);
    F32 *g_plane = write_base + (1 * frame_size);
    F32 *b_plane = write_base + (2 * frame_size);

    F32 mean_r = model.clip->image_mean[0], inv_std_r = 1 / model.clip->image_std[0];
    F32 mean_g = model.clip->image_mean[1], inv_std_g = 1 / model.clip->image_std[1];
    F32 mean_b = model.clip->image_mean[2], inv_std_b = 1 / model.clip->image_std[2];

    // TODO: Find some SIMD or hacky way to speed it up. This takes ~4s to process per image
    // Currently implemented solutions:
    // - Loop unrolling of 4 loops
    // - Pre-calculate indices
    for (U32 i = 0; i < frame_size; i += 4)
    {
        U32 src0 = 3 * (i + 0);
        U32 src1 = 3 * (i + 1);
        U32 src2 = 3 * (i + 2);
        U32 src3 = 3 * (i + 3);

        // Red Plane
        r_plane[i + 0] = (resized[src0 + 0] - mean_r) * inv_std_r;
        r_plane[i + 1] = (resized[src1 + 0] - mean_r) * inv_std_r;
        r_plane[i + 2] = (resized[src2 + 0] - mean_r) * inv_std_r;
        r_plane[i + 3] = (resized[src3 + 0] - mean_r) * inv_std_r;

        // Green Plane
        g_plane[i + 0] = (resized[src0 + 1] - mean_g) * inv_std_g;
        g_plane[i + 1] = (resized[src1 + 1] - mean_g) * inv_std_g;
        g_plane[i + 2] = (resized[src2 + 1] - mean_g) * inv_std_g;
        g_plane[i + 3] = (resized[src3 + 1] - mean_g) * inv_std_g;

        // Blue Plane
        b_plane[i + 0] = (resized[src0 + 2] - mean_b) * inv_std_b;
        b_plane[i + 1] = (resized[src1 + 2] - mean_b) * inv_std_b;
        b_plane[i + 2] = (resized[src2 + 2] - mean_b) * inv_std_b;
        b_plane[i + 3] = (resized[src3 + 2] - mean_b) * inv_std_b;

        // batch_data[batch_idx * image_size + 0 * frame_size + i] = ((resized[3 * i + 0] - mean[0]) * inv_std[0]); /* red */
        // batch_data[batch_idx * image_size + 1 * frame_size + i] = ((resized[3 * i + 1] - mean[1]) * inv_std[1]); /* green */
        // batch_data[batch_idx * image_size + 2 * frame_size + i] = ((resized[3 * i + 2] - mean[2]) * inv_std[2]); /* blue */
    }

    temp_end(scratch);
}

void model_insert_embedding_impl(Arena *arena)
{
    arena_clear(arena);
    ImageRow *inserted = NULL;

    // NOTE: This only needs id and path, so maybe ImageRow is redundant for it
    sqlite3_stmt *stmt = db_prepare("SELECT id, path FROM Images WHERE embedding IS NULL;");
    db_run_stmt(stmt, 1, push_imagerow, &inserted, arena);

    if (!model_arena)
        arena_alloc(MB(100), model_arena);
    if (!model.clip)
    {
        model.clip = push_struct(model_arena, clip_ctx);
        clip_model_load(model_arena, model.clip, MODEL_PATH);
    }

    clip_vision_model *vision_model = &model.clip->vision_model;
    clip_vision_hparams hparams = vision_model->hparams;
    ggml_cgraph *graph = build_image_encode_graph(vision_model, model.clip, EMBED_BATCH_SIZE);
    ggml_gallocr_t allocr = ggml_gallocr_new(ggml_backend_get_default_buffer_type(vision_model->backend));
    ggml_gallocr_alloc_graph(allocr, graph);

    // mscbl_log_dbg("Graph context mem usage: %zu/%zu (%.2f%%)",
    //               ggml_used_mem(graph_ctx), ggml_get_mem_size(graph_ctx),
    //               (float)ggml_used_mem(graph_ctx) / (float)ggml_get_mem_size(graph_ctx) * 100.0f);

    S32 image_size = hparams.image_size;
    S32 frame_size = image_size * image_size;
    F32 *batch_data = push_array0(arena, (frame_size * 3 * EMBED_BATCH_SIZE), F32);

    Semaphore batch_sem = os_semaphore_alloc(0, S32_MAX);
    stmt = db_prepare("UPDATE Images SET embedding = ? WHERE id = ?;");

    for (S64 batch = 0; batch < da_getsize(inserted); batch += EMBED_BATCH_SIZE)
    {
        PERF_BEGIN(batch);
        U64 batch_size = MIN(EMBED_BATCH_SIZE, da_getsize(inserted) - batch);
        S64 batch_var = batch_size - 1;

        for (U64 i = 1; i < batch_size; i++)
        {
            preprocess_params *params = push_struct(arena, preprocess_params);
            *params = {
                .path = inserted[batch + i].path,
                .write_base = batch_data + (i * frame_size * 3)};

            AsyncTask task = {
                .func = preprocess_image,
                .data = {
                    .kind = TPData_ANY,
                    .val_any = params},
                .batch_size = &batch_var,
                .batch_complete = batch_sem};
            threadpool_enqueue(task);
        }

        Temp scratch = temp_begin(arena);

        S32 image_size = hparams.image_size;
        U32 frame_size = image_size * image_size;
        F32 *resized = push_array(scratch.arena, frame_size * 3, F32);

        S32 w, h;
        F32 *image_data = stbi_loadf(CStrCast(inserted[batch].path), &w, &h, NULL, 3);
        Assert(image_data, "image data is NULL (%.*s)", StringSpr(inserted[batch].path));
        stbir_resize_float_linear(image_data, w, h, 0, resized, image_size, image_size, 0, STBIR_RGB);
        stbi_image_free(image_data);

        F32 *r_plane = batch_data + (0 * frame_size);
        F32 *g_plane = batch_data + (1 * frame_size);
        F32 *b_plane = batch_data + (2 * frame_size);

        F32 mean_r = model.clip->image_mean[0], inv_std_r = 1 / model.clip->image_std[0];
        F32 mean_g = model.clip->image_mean[1], inv_std_g = 1 / model.clip->image_std[1];
        F32 mean_b = model.clip->image_mean[2], inv_std_b = 1 / model.clip->image_std[2];

        for (U32 i = 0; i < frame_size; i += 4)
        {
            U32 src0 = 3 * (i + 0);
            U32 src1 = 3 * (i + 1);
            U32 src2 = 3 * (i + 2);
            U32 src3 = 3 * (i + 3);

            // Red Plane
            r_plane[i + 0] = (resized[src0 + 0] - mean_r) * inv_std_r;
            r_plane[i + 1] = (resized[src1 + 0] - mean_r) * inv_std_r;
            r_plane[i + 2] = (resized[src2 + 0] - mean_r) * inv_std_r;
            r_plane[i + 3] = (resized[src3 + 0] - mean_r) * inv_std_r;

            // Green Plane
            g_plane[i + 0] = (resized[src0 + 1] - mean_g) * inv_std_g;
            g_plane[i + 1] = (resized[src1 + 1] - mean_g) * inv_std_g;
            g_plane[i + 2] = (resized[src2 + 1] - mean_g) * inv_std_g;
            g_plane[i + 3] = (resized[src3 + 1] - mean_g) * inv_std_g;

            // Blue Plane
            b_plane[i + 0] = (resized[src0 + 2] - mean_b) * inv_std_b;
            b_plane[i + 1] = (resized[src1 + 2] - mean_b) * inv_std_b;
            b_plane[i + 2] = (resized[src2 + 2] - mean_b) * inv_std_b;
            b_plane[i + 3] = (resized[src3 + 2] - mean_b) * inv_std_b;
        }

        temp_end(scratch);
        os_semaphore_take(batch_sem, U64_MAX);

        Embedding embeddings = clip_get_image_embedding(arena, model.clip, graph, batch_data, EMBED_BATCH_SIZE);
        for (U32 i = 0; i < batch_size; i++)
        {
            F32 *embedding = embeddings.vector + i * embeddings.size;
            U64 size = embeddings.size * sizeof(F32);
            sqlite3_bind_blob(stmt, 1, embedding, size, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 2, inserted[batch + i].id);
            db_run_stmt(stmt);
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
        }
    }

    sqlite3_finalize(stmt);
    os_semaphore_release(batch_sem);

    void *graph_mem = ggml_get_mem_buffer(vision_model->graph_ctx);
    ggml_free(vision_model->graph_ctx);
    ggml_gallocr_free(allocr);

    U64 mem_size = ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead();
    vision_model->graph_ctx = ggml_init({.mem_size = mem_size,
                                         .mem_buffer = graph_mem,
                                         .no_alloc = true});
}

struct model_embed
{
    B32 is_running;
    B32 needs_rerun;
};

global_v model_embed m_embed = {0};

ThreadFunc(model_insert_embedding)
{
    if (ins_atomic_u32_eval_cond_assign(&m_embed.is_running, 1, 0) == 0)
    {
        // This thread is first to run it
        do
        {
            ins_atomic_u32_eval_assign(&m_embed.needs_rerun, 0);
            // Embed images here
            model_insert_embedding_impl(arena);
        } while (ins_atomic_u32_eval(&m_embed.needs_rerun) != 0);
        ins_atomic_u32_eval_assign(&m_embed.is_running, 0);
        return;
    }
    // Other thread already running this function
    ins_atomic_u32_eval_assign(&m_embed.needs_rerun, 1);
}

DBStmtCbk(print_dist)
{
    mscbl_log_dbg("id: %zu, path: %s, distance: %.6f", sqlite3_column_int64(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_double(stmt, 2));
}

Embedding model_embed_text(Arena *arena, String text)
{
    if (!text.size)
        return {0};

    if (!model_arena)
        arena_alloc(MB(100), model_arena);
    if (!model.clip)
    {
        model.clip = push_struct(model_arena, clip_ctx);
        clip_model_load(model_arena, model.clip, MODEL_PATH);
    }

    clip_tokens tokens;
    clip_tokenize(model.clip, &text, &tokens);

    Embedding embedding = clip_get_text_embedding(arena, model.clip, &tokens);

    // sqlite3_stmt *stmt = db_prepare("SELECT id, path, distance_cosine_f32(embedding, ?) AS distance FROM Images WHERE embedding IS NOT NULL ORDER BY distance ASC LIMIT 10;");
    // sqlite3_bind_blob(stmt, 1, embedding, 512 * sizeof(F32), SQLITE_STATIC);
    // mscbl_log_dbg("Returned %zu rows", db_run_stmt(stmt, 1, print_dist));
    return embedding;
}
