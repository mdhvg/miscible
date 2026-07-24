// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "ggml.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

#include "inference/inference.h"
#include "network/download.h"
#include "base/base_core.h"
#include "base/string.h"
#include "config.h"
#include "ui/ui_core.h"
#include "base/log.h"
#include "scan/scan.h"
#include "os/os_inc.h"
#include "base/arena.h"
#include "base/array.h"
#include "db/db_helpers.h"
#include "inference/clip.h"
#include "base/threadpool.h"
#include "inference/model.h"

CLIPModel model = {0};
Arena *model_arena = NULL;

#define EMBED_BATCH_SIZE 8
#define DOWNLOAD_RETRIES 10

ThreadFunc(preprocess_image)
{
    Assert(args[0].kind == TPData_String, "wrong datatype");
    Assert(args[1].kind == TPData_Any, "wrong datatype");
    Assert(args[2].kind == TPData_Any, "wrong datatype");
    String image_path = args[0].val_str;
    F32 *write_base = (F32 *)args[1].val_any;
    VisionModelConfig *config = (VisionModelConfig *)args[2].val_any;

    ArenaScoped(arena)
    {
        perf_beg(preprocess);
        S32 image_size = config->input_size;
        U32 frame_size = image_size * image_size;
        U8 *resized = push_array(arena, frame_size * 3, U8);

        S32 w, h;
        U8 *image_data = stbi_load(CStrCast(image_path), &w, &h, NULL, 3);
        Assert(image_data, "image data is NULL (%.*s)", StringSpr(image_path));

        S32 crop_x = 0, crop_y = 0,
            crop_w = w, crop_h = h;

        if (w > h)
        {
            // Landscape
            crop_w = h;
            crop_x = (w - h) / 2;
        }
        else if (h > w)
        {
            // Portrait
            crop_h = w;
            crop_y = (h - w) / 2;
        }

        U8 *cropped_source = image_data + (crop_y * w + crop_x) * 3;
        stbir_resize_uint8_linear(cropped_source, crop_w, crop_h, w * 3, resized, image_size, image_size, 0, STBIR_RGB);
        stbi_image_free(image_data);

        F32 *__restrict r_plane = write_base + (0 * frame_size);
        F32 *__restrict g_plane = write_base + (1 * frame_size);
        F32 *__restrict b_plane = write_base + (2 * frame_size);

        const F32 mean_r = config->mean[0], inv_std_r = 1 / config->std_dev[0];
        const F32 mean_g = config->mean[1], inv_std_g = 1 / config->std_dev[1];
        const F32 mean_b = config->mean[2], inv_std_b = 1 / config->std_dev[2];

        const F32 rescale = config->rescale_factor;

        // TODO: Find some SIMD or hacky way to speed it up. Currently implemented solutions:
        // - Loop unrolling of 8 loops
        // - Pre-calculate indices
        for (U32 i = 0; i < frame_size; i += 8)
        {
            U32 src0 = 3 * (i + 0);
            U32 src1 = 3 * (i + 1);
            U32 src2 = 3 * (i + 2);
            U32 src3 = 3 * (i + 3);
            U32 src4 = 3 * (i + 4);
            U32 src5 = 3 * (i + 5);
            U32 src6 = 3 * (i + 6);
            U32 src7 = 3 * (i + 7);

            // Red Plane
            r_plane[i + 0] = ((F32)resized[src0 + 0] * rescale - mean_r) * inv_std_r;
            r_plane[i + 1] = ((F32)resized[src1 + 0] * rescale - mean_r) * inv_std_r;
            r_plane[i + 2] = ((F32)resized[src2 + 0] * rescale - mean_r) * inv_std_r;
            r_plane[i + 3] = ((F32)resized[src3 + 0] * rescale - mean_r) * inv_std_r;
            r_plane[i + 4] = ((F32)resized[src4 + 0] * rescale - mean_r) * inv_std_r;
            r_plane[i + 5] = ((F32)resized[src5 + 0] * rescale - mean_r) * inv_std_r;
            r_plane[i + 6] = ((F32)resized[src6 + 0] * rescale - mean_r) * inv_std_r;
            r_plane[i + 7] = ((F32)resized[src7 + 0] * rescale - mean_r) * inv_std_r;

            // Green Plane
            g_plane[i + 0] = ((F32)resized[src0 + 1] * rescale - mean_g) * inv_std_g;
            g_plane[i + 1] = ((F32)resized[src1 + 1] * rescale - mean_g) * inv_std_g;
            g_plane[i + 2] = ((F32)resized[src2 + 1] * rescale - mean_g) * inv_std_g;
            g_plane[i + 3] = ((F32)resized[src3 + 1] * rescale - mean_g) * inv_std_g;
            g_plane[i + 4] = ((F32)resized[src4 + 1] * rescale - mean_g) * inv_std_g;
            g_plane[i + 5] = ((F32)resized[src5 + 1] * rescale - mean_g) * inv_std_g;
            g_plane[i + 6] = ((F32)resized[src6 + 1] * rescale - mean_g) * inv_std_g;
            g_plane[i + 7] = ((F32)resized[src7 + 1] * rescale - mean_g) * inv_std_g;

            // Blue Plane
            b_plane[i + 0] = ((F32)resized[src0 + 2] * rescale - mean_b) * inv_std_b;
            b_plane[i + 1] = ((F32)resized[src1 + 2] * rescale - mean_b) * inv_std_b;
            b_plane[i + 2] = ((F32)resized[src2 + 2] * rescale - mean_b) * inv_std_b;
            b_plane[i + 3] = ((F32)resized[src3 + 2] * rescale - mean_b) * inv_std_b;
            b_plane[i + 4] = ((F32)resized[src4 + 2] * rescale - mean_b) * inv_std_b;
            b_plane[i + 5] = ((F32)resized[src5 + 2] * rescale - mean_b) * inv_std_b;
            b_plane[i + 6] = ((F32)resized[src6 + 2] * rescale - mean_b) * inv_std_b;
            b_plane[i + 7] = ((F32)resized[src7 + 2] * rescale - mean_b) * inv_std_b;
        }
        perf_end(preprocess);
    }
}

void model_insert_embedding_impl(Arena *arena)
{
    if (inference_state_get() != InferenceState_Ready)
        return;

    arena_clear(arena);

    ImageRow *inserted = NULL;
    // TODO: This only needs id and path, so maybe ImageRow is redundant for it
    sqlite3_stmt *stmt = db_prepare("SELECT id, path FROM Images WHERE embedding IS NULL;");
    db_run_stmt(stmt, 1, push_imagerow, &inserted, arena);

    Semaphore batch_sem = os_semaphore_init(0, S32_MAX);
    stmt = db_prepare("UPDATE Images SET embedding = ? WHERE id = ?;");
    VisionModelConfig *config = inference_preprocess_get();
    S32 image_size = config->input_size;
    U32 frame_size = image_size * image_size;
    F32 *batch_data = push_array0(arena, (frame_size * 3 * EMBED_BATCH_SIZE), F32);

    for (S64 batch = 0; batch < da_getsize(inserted); batch += EMBED_BATCH_SIZE)
    {
        U64 batch_size = MIN(EMBED_BATCH_SIZE, da_getsize(inserted) - batch);
        S64 batch_var = batch_size - 1;

        if (batch_size > 1)
        {
            for (U64 i = 1; i < batch_size; i++)
            {
                AsyncTask task = {
                    .func = preprocess_image,
                    .args = {
                        {.kind = TPData_String, .val_str = inserted[batch + i].path},
                        {.kind = TPData_Any, .val_any = batch_data + (i * frame_size * 3)},
                        {.kind = TPData_Any, .val_any = config},
                    },
                    .batch_size = &batch_var,
                    .batch_complete = batch_sem};
                threadpool_enqueue(TaskPriority_Low, task);
            }
        }

        ArenaScoped(arena)
        {
            perf_beg(preprocess);
            U8 *resized = push_array(arena, frame_size * 3, U8);

            S32 w, h;
            U8 *image_data = stbi_load(CStrCast(inserted[batch].path), &w, &h, NULL, 3);
            Assert(image_data, "image data is NULL (%.*s)", StringSpr(inserted[batch].path));

            S32 crop_x = 0, crop_y = 0,
                crop_w = w, crop_h = h;

            if (w > h)
            {
                // Landscape
                crop_w = h;
                crop_x = (w - h) / 2;
            }
            else if (h > w)
            {
                // Portrait
                crop_h = w;
                crop_y = (h - w) / 2;
            }

            U8 *cropped_source = image_data + (crop_y * w + crop_x) * 3;
            stbir_resize_uint8_linear(cropped_source, crop_w, crop_h, w * 3, resized, image_size, image_size, 0, STBIR_RGB);
            stbi_image_free(image_data);

            F32 *__restrict r_plane = batch_data + (0 * frame_size);
            F32 *__restrict g_plane = batch_data + (1 * frame_size);
            F32 *__restrict b_plane = batch_data + (2 * frame_size);

            const F32 mean_r = config->mean[0], inv_std_r = 1 / config->std_dev[0];
            const F32 mean_g = config->mean[1], inv_std_g = 1 / config->std_dev[1];
            const F32 mean_b = config->mean[2], inv_std_b = 1 / config->std_dev[2];

            const F32 rescale = config->rescale_factor;

            // TODO: Find some SIMD or hacky way to speed it up. Currently implemented solutions:
            // - Loop unrolling of 8 loops
            // - Pre-calculate indices
            for (U32 i = 0; i < frame_size; i += 8)
            {
                U32 src0 = 3 * (i + 0);
                U32 src1 = 3 * (i + 1);
                U32 src2 = 3 * (i + 2);
                U32 src3 = 3 * (i + 3);
                U32 src4 = 3 * (i + 4);
                U32 src5 = 3 * (i + 5);
                U32 src6 = 3 * (i + 6);
                U32 src7 = 3 * (i + 7);

                // Red Plane
                r_plane[i + 0] = ((F32)resized[src0 + 0] * rescale - mean_r) * inv_std_r;
                r_plane[i + 1] = ((F32)resized[src1 + 0] * rescale - mean_r) * inv_std_r;
                r_plane[i + 2] = ((F32)resized[src2 + 0] * rescale - mean_r) * inv_std_r;
                r_plane[i + 3] = ((F32)resized[src3 + 0] * rescale - mean_r) * inv_std_r;
                r_plane[i + 4] = ((F32)resized[src4 + 0] * rescale - mean_r) * inv_std_r;
                r_plane[i + 5] = ((F32)resized[src5 + 0] * rescale - mean_r) * inv_std_r;
                r_plane[i + 6] = ((F32)resized[src6 + 0] * rescale - mean_r) * inv_std_r;
                r_plane[i + 7] = ((F32)resized[src7 + 0] * rescale - mean_r) * inv_std_r;

                // Green Plane
                g_plane[i + 0] = ((F32)resized[src0 + 1] * rescale - mean_g) * inv_std_g;
                g_plane[i + 1] = ((F32)resized[src1 + 1] * rescale - mean_g) * inv_std_g;
                g_plane[i + 2] = ((F32)resized[src2 + 1] * rescale - mean_g) * inv_std_g;
                g_plane[i + 3] = ((F32)resized[src3 + 1] * rescale - mean_g) * inv_std_g;
                g_plane[i + 4] = ((F32)resized[src4 + 1] * rescale - mean_g) * inv_std_g;
                g_plane[i + 5] = ((F32)resized[src5 + 1] * rescale - mean_g) * inv_std_g;
                g_plane[i + 6] = ((F32)resized[src6 + 1] * rescale - mean_g) * inv_std_g;
                g_plane[i + 7] = ((F32)resized[src7 + 1] * rescale - mean_g) * inv_std_g;

                // Blue Plane
                b_plane[i + 0] = ((F32)resized[src0 + 2] * rescale - mean_b) * inv_std_b;
                b_plane[i + 1] = ((F32)resized[src1 + 2] * rescale - mean_b) * inv_std_b;
                b_plane[i + 2] = ((F32)resized[src2 + 2] * rescale - mean_b) * inv_std_b;
                b_plane[i + 3] = ((F32)resized[src3 + 2] * rescale - mean_b) * inv_std_b;
                b_plane[i + 4] = ((F32)resized[src4 + 2] * rescale - mean_b) * inv_std_b;
                b_plane[i + 5] = ((F32)resized[src5 + 2] * rescale - mean_b) * inv_std_b;
                b_plane[i + 6] = ((F32)resized[src6 + 2] * rescale - mean_b) * inv_std_b;
                b_plane[i + 7] = ((F32)resized[src7 + 2] * rescale - mean_b) * inv_std_b;
            }
            perf_end(preprocess);
        }

        if (batch_size > 1)
            os_semaphore_pop(batch_sem, U64_MAX);

        Embedding embeddings = inference_vision_embedding(arena, batch_data, batch_size);
        for (U32 i = 0; i < batch_size; i++)
        {
            F32 *embedding = embeddings.vector + i * embeddings.size;
            U64 size_in_bytes = embeddings.size * sizeof(F32);
            sqlite3_bind_blob(stmt, 1, embedding, size_in_bytes, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 2, inserted[batch + i].id);
            db_run_stmt(stmt);
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
        }
    }

    sqlite3_finalize(stmt);
    os_semaphore_destroy(batch_sem);
}

// B32 model_load(Arena *arena, GGMLConfig *model_cfg)
// {
//     S64 url_idx = 0;
//     B32 model_exist = 0;
//
//     Result res = ResultSuccess();
//     ArenaScoped(arena)
//     {
//         model_exist = os_path_exists(model_cfg->path, &res);
//         CheckAndClearResult(res);
//         if (!model_exist)
//         {
//             // NOTE: if model doesn't exist, download it. for that, it needs to have the manifest object
//             DownloadArgs args = {
//                 .links = model_cfg->model_url,
//                 .filepath = model_cfg->path,
//                 .file_size = model_cfg->filesize,
//                 .file_hash = model_cfg->model_hash,
//             };
//             res = download_file(arena, args);
//             CheckAndClearResult(res);
//         }
//
//         // TODO: make this call more generic for any kind of model
//         if (!model_arena)
//             arena_alloc(MB(100), model_arena);
//         model.clip = push_struct(model_arena, clip_ctx);
//         res = clip_model_load(model_arena, model.clip, model_cfg->path);
//         CheckAndClearResult(res);
//
//     Cleanup:;
//     }
//
//     if (!res.success)
//         ui_push_message(res);
//
//     return res.success;
// }

Result model_download_files(Arena *arena, RemoteFileArr files, String model_base)
{
    Result res = ResultSuccess();
    for (S64 fi = 0; fi < da_getsize(files); fi++)
    {
        RemoteFile file = files[fi];
        StringBuilder file_path = string_init(arena, StringCast(model_base));
        path_join(&file_path, file.name);

        DownloadArgs args = {
            .link = file.url,
            .filepath = StringCast(file_path),
            .file_size = file.size,
            .file_hash = file.hash};
        res = download_file(arena, args);

        if (!res.success)
            return res;
    }

    return res;
}

struct ImageEmbeddingState
{
    B32 is_running;
    B32 needs_rerun;
};

static ImageEmbeddingState image_embed_state = {.is_running = 0};

ThreadFunc(model_insert_embedding)
{
    if (ins_atomic_u32_eval_cond_assign(&image_embed_state.is_running, 1, 0) != 0)
    {
        // Function already running
        ins_atomic_u32_eval_assign(&image_embed_state.needs_rerun, 1);
        return;
    }

    do
    {
        ins_atomic_u32_eval_assign(&image_embed_state.needs_rerun, 0);
        model_insert_embedding_impl(arena);
    } while (ins_atomic_u32_eval(&image_embed_state.needs_rerun) != 0);
    ins_atomic_u32_eval_assign(&image_embed_state.is_running, 0);
}

DBStmtCbk(print_dist)
{
    mscbl_log_info(
        "id: %zu, path: %s, distance: %.6f",
        sqlite3_column_int64(stmt, 0),
        sqlite3_column_text(stmt, 1),
        sqlite3_column_double(stmt, 2));
}

Embedding model_embed_text(Arena *arena, String text)
{
    if (!text.size)
        return {0};

    clip_tokens tokens;
    clip_tokenize(model.clip, &text, &tokens);

    Embedding embedding = clip_get_text_embedding(arena, model.clip, &tokens);

    // sqlite3_stmt *stmt = db_prepare("SELECT id, path, distance_cosine_f32(embedding, ?) AS distance FROM Images WHERE embedding IS NOT NULL ORDER BY distance ASC LIMIT 10;");
    // sqlite3_bind_blob(stmt, 1, embedding, 512 * sizeof(F32), SQLITE_STATIC);
    // mscbl_log_info("Returned %zu rows", db_run_stmt(stmt, 1, print_dist));
    return embedding;
}
