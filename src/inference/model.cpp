#include "base/string.h"
#include "config.h"
#include "ggml.h"
#include "sha2.h"
#include "libfyaml.h"
#include "curl/curl.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

#include "yaml.h"
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

    ArenaScoped(arena)
    {
        clip_vision_model *vision_model = &model.clip->vision_model;
        clip_vision_hparams hparams = vision_model->hparams;
        S32 image_size = hparams.image_size;
        U32 frame_size = image_size * image_size;
        F32 *resized = push_array(arena, frame_size * 3, F32);

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
    }
}

void model_insert_embedding_impl(Arena *arena)
{
    arena_clear(arena);
    ImageRow *inserted = NULL;

    // NOTE: This only needs id and path, so maybe ImageRow is redundant for it
    sqlite3_stmt *stmt = db_prepare("SELECT id, path FROM Images WHERE embedding IS NULL;");
    db_run_stmt(stmt, 1, push_imagerow, &inserted, arena);

    ModelConfig *clip_cfg = &mscbl_config.model_group.clip_model;

    if (!model_arena)
        arena_alloc(MB(100), model_arena);
    if (!model_clip_exists(arena))
    {
        model_download(arena, clip_cfg);
    }
    if (!model.clip)
    {
        model.clip = push_struct(model_arena, clip_ctx);
        clip_model_load(model_arena, model.clip, clip_cfg->path);
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
            threadpool_enqueue(TaskPriority_Low, task);
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
        // NOTE: This thread is first to run it
        do
        {
            ins_atomic_u32_eval_assign(&m_embed.needs_rerun, 0);
            // NOTE: Embed images here
            model_insert_embedding_impl(arena);
        } while (ins_atomic_u32_eval(&m_embed.needs_rerun) != 0);
        ins_atomic_u32_eval_assign(&m_embed.is_running, 0);
        return;
    }
    // NOTE: Other thread already running this function
    ins_atomic_u32_eval_assign(&m_embed.needs_rerun, 1);
}

struct Block
{
    U64 start;
    U64 size;
    U8 hash[SHA512_DIGEST_SIZE];
};

struct Manifest
{
    String filename;
    U64 total_size;
    Block *blocks;
};

static U64 manifest_cb(U8 *data, U64 n, U64 l, void *userp)
{
    StringBuilder *base = (StringBuilder *)userp;
    String view = {(U8 *)data, n * l};
    string_push(base, view);
    return n * l;
}

void model_parse_manifest(Arena *arena, String content, Manifest *manifest)
{
    fy_document *fyd = fy_document_build_from_string(NULL, CStrCast(content), content.size);
    Assert(fyd, "fyd is NULL");
    fy_node *root = fy_document_root(fyd);

    U8 *str_buf = push_array(arena, KB(4), U8);

    manifest->filename = yaml_scan_string(arena, root, str_buf, "/filename");
    manifest->total_size = yaml_scan_int(root, "/total_size");

    fy_node *blocks = fy_node_by_path(root, "/blocks", FY_NT, FYNWF_FOLLOW);
    Assert(!fy_node_sequence_is_empty(blocks), "blocks array empty");

    S64 block_count = fy_node_sequence_item_count(blocks);
    da_setcap(arena, manifest->blocks, block_count);

    fy_node *block_item = NULL;
    void *pre = NULL;
    while ((block_item = fy_node_sequence_iterate(blocks, &pre)))
    {
        Block block = {0};
        block.start = yaml_scan_int(block_item, "/start");
        block.size = yaml_scan_int(block_item, "/size");
        yaml_scan_hash(block_item, str_buf, block.hash, SHA512_DIGEST_SIZE, "/hash");
        da_push(arena, manifest->blocks, block);
    }
}

// TODO: switch from asserts regarding http/s errors to false result
B32 model_download_manifest(Arena *arena, Manifest *manifest, String url, String path)
{
    CURL *curl = curl_easy_init();
    Assert(curl, "curl is NULL");

    StringBuilder base = string_empty(arena, KB(4));

    Assert(curl_easy_setopt(curl, CURLOPT_URL, CStrCast(url)) == CURLE_OK, "curl error");
    Assert(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK, "curl error");
    Assert(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &base) == CURLE_OK, "curl error");
    Assert(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, manifest_cb) == CURLE_OK, "curl error");

    if (curl_easy_perform(curl) != CURLE_OK)
        return 0;

    // mscbl_log_dbg("%.*s", StringSpr(base));
    // mscbl_log_dbg("size: %zu, capacity: %zu, used: %.2f%%",
    //               base.size,
    //               base.capacity,
    //               (F32)base.size / (F32)base.capacity * 100.0f);

    U8 *manifest_hash_actual = mscbl_config.model_group.clip_model.manifest_hash;
    U8 manifest_hash_found[SHA512_DIGEST_SIZE] = {0};
    sha512_ctx ctx = {0};
    sha512_init(&ctx);
    sha512_update(&ctx, base.v, base.size);
    sha512_final(&ctx, manifest_hash_found);
    Assert(!memcmp(manifest_hash_found, manifest_hash_actual, SHA512_DIGEST_SIZE), "manifest corrupt");

    FileHandle file = os_file_open(path, FileAccess_Write, FileMode_OpenAlways);
    os_file_write(file, StringSpr(base));
    os_file_close(file);

    model_parse_manifest(arena, StringCast(base), manifest);

    return 1;
}

void model_read_manifest(Arena *arena, Manifest *manifest, String path)
{
    FileHandle file = os_file_open(path, FileAccess_Read, FileMode_OpenAlways);
    U64 manifest_file_size = os_file_size(file);
    U8 *buffer = push_array(arena, manifest_file_size, U8);
    os_file_read(file, manifest_file_size, buffer);
    os_file_close(file);

    String content = sv(buffer, manifest_file_size);
    model_parse_manifest(arena, content, manifest);
}

struct download_params
{
    String model_url;
    FileHandle file_desc;
    S64 idx;
    U8 *state;
    Block block;
};

struct cbk_params
{
    U8 *buffer;
    U64 used;
};

static U64 download_cb(U8 *data, U64 n, U64 l, void *userp)
{
    cbk_params *params = (cbk_params *)userp;

    U8 *bytes_p = params->buffer;
    U64 used = params->used;

    MemoryCopy(bytes_p + used, data, n * l);
    params->used += n * l;

    return n * l;
}

ThreadFunc(download_worker)
{
    arena_clear(arena);
    Assert(data.kind == TPData_ANY, "wrong datatype");
    download_params *params0 = (download_params *)data.val_any;

    U64 start = params0->block.start;
    U64 end = start + params0->block.size - 1;
    StringBuilder range = string_empty(arena, 256);
    string_format(&range, "%zu-%zu", start, end);

    String model_url = params0->model_url;
    FileHandle file_desc = params0->file_desc;

    U8 *array = push_array(arena, params0->block.size, U8);
    cbk_params params1 = {.buffer = array, .used = 0};

    CURL *curl = curl_easy_init();
    Assert(curl, "curl is NULL");

    CURLcode res;
    res = curl_easy_setopt(curl, CURLOPT_URL, CStrCast(model_url));
    Assert(res == CURLE_OK, "curl fail");
    curl_easy_setopt(curl, CURLOPT_RANGE, CStrCast(range));
    Assert(res == CURLE_OK, "curl fail");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    Assert(res == CURLE_OK, "curl fail");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &params1);
    Assert(res == CURLE_OK, "curl fail");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_cb);
    Assert(res == CURLE_OK, "curl fail"); // TODO: replace with some return or something

    res = curl_easy_perform(curl);
    Assert(res == CURLE_OK, "curl fail");

    curl_easy_cleanup(curl);

    Assert(params1.used == params0->block.size, "not filled completely");

    sha512_ctx ctx = {0};
    U8 calculated_hash[SHA512_DIGEST_SIZE] = {0};
    sha512_init(&ctx);
    sha512_update(&ctx, array, params1.used);
    sha512_final(&ctx, calculated_hash);

    Assert(!memcmp(params0->block.hash, calculated_hash, SHA512_DIGEST_SIZE), "hashes don't match"); // TODO: replace with something to set bitfield
    BitFieldSet(params0->state, params0->idx);

    os_file_write(file_desc, params1.used, params1.buffer, params0->block.start);

    // fseek(file, start, SEEK_SET);
    // fwrite(array, 1, used, file);
}

B32 model_download_impl(Arena *arena, Manifest *manifest, String model_url, ModelConfig *model_cfg)
{
    S64 block_count = da_getsize(manifest->blocks);
    U64 state_array_size = ToCeilInt(block_count, (sizeof(U8) * 8));

    StringBuilder statefile_path = string_init(arena, model_cfg->path);
    string_push(&statefile_path, ".state");

    FileHandle statefile_desc = os_file_open(StringCast(statefile_path), FileAccess_Read | FileAccess_Write, FileMode_OpenAlways);
    OSMmap map = os_file_map(statefile_desc, state_array_size);
    U8 *state_array = (U8 *)os_map_get_data(map);

    StringBuilder tempfile_path = string_init(arena, model_cfg->path);
    string_push(&tempfile_path, ".tmp");
    FileHandle tempfile_desc = os_file_open(StringCast(tempfile_path), FileAccess_Read | FileAccess_Write, FileMode_OpenAlways);

    S64 batch_size = block_count;
    for (S64 i = 0; i < block_count; i++)
    {
        if (BitFieldGet(state_array, i))
            batch_size--;
    }

    if (batch_size)
    {
        Semaphore wait = os_semaphore_alloc(0, S32_MAX);
        for (S64 i = 0; i < block_count; i++)
        {
            if (BitFieldGet(state_array, i))
                continue;
            download_params *params = push_struct(arena, download_params);
            *params = {.model_url = model_url,
                       .file_desc = tempfile_desc,
                       .idx = i,
                       .state = state_array,
                       .block = manifest->blocks[i]};
            AsyncTask task = {.func = download_worker,
                              .data =
                                  {
                                      .kind = TPData_ANY,
                                      .val_any = params,
                                  },
                              .batch_size = &batch_size,
                              .batch_complete = wait};
            threadpool_enqueue(TaskPriority_Low, task);
        }

        os_semaphore_take(wait, U64_MAX);
        os_semaphore_release(wait);
    }

    B32 res = 1;
    for (S64 i = 0; i < block_count; i++)
    {
        res &= BitFieldGet(state_array, i);
    }

    os_file_unmap(map);
    os_file_close(statefile_desc);
    os_file_close(tempfile_desc);

    return res;
}

void model_download(Arena *arena, ModelConfig *model_cfg)
{
    ArenaScoped(arena)
    {
        StringBuilder manifest_path = string_init(arena, model_cfg->path);
        string_push(&manifest_path, ".yaml");

        Manifest manifest = {0};

        if (os_path_exists(StringCast(manifest_path)))
        {
            // TODO: load manifest from file
            model_read_manifest(arena, &manifest, StringCast(manifest_path));
        }
        else
        {
            B32 manifest_downloaded = 0;
            for (S64 i = 0; i < da_getsize(model_cfg->manifest_url) && !manifest_downloaded; i++)
            {
                String manifest_url = model_cfg->manifest_url[i];
                manifest_downloaded |= model_download_manifest(arena, &manifest, manifest_url, StringCast(manifest_path));
            }
            Assert(manifest_downloaded, "all links failed to download manifest");
        }

        B32 model_downloaded = 0;
        for (S64 i = 0; i < da_getsize(model_cfg->model_url) && !model_downloaded; i++)
        {
            String model_url = model_cfg->model_url[i];
            model_downloaded |= model_download_impl(arena, &manifest, model_url, model_cfg);
        }
        if (model_downloaded)
        {
            StringBuilder tempfile_path = string_init(arena, model_cfg->path);
            string_push(&tempfile_path, ".tmp");
            os_file_rename(StringCast(tempfile_path), model_cfg->path);
        }
        Assert(model_downloaded, "all links failed to download model");
    }
}

DBStmtCbk(print_dist)
{
    mscbl_log_dbg(
        "id: %zu, path: %s, distance: %.6f",
        sqlite3_column_int64(stmt, 0),
        sqlite3_column_text(stmt, 1),
        sqlite3_column_double(stmt, 2));
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
        clip_model_load(model_arena, model.clip, mscbl_config.model_group.clip_model.path);
    }

    clip_tokens tokens;
    clip_tokenize(model.clip, &text, &tokens);

    Embedding embedding = clip_get_text_embedding(arena, model.clip, &tokens);

    // sqlite3_stmt *stmt = db_prepare("SELECT id, path, distance_cosine_f32(embedding, ?) AS distance FROM Images WHERE embedding IS NOT NULL ORDER BY distance ASC LIMIT 10;");
    // sqlite3_bind_blob(stmt, 1, embedding, 512 * sizeof(F32), SQLITE_STATIC);
    // mscbl_log_dbg("Returned %zu rows", db_run_stmt(stmt, 1, print_dist));
    return embedding;
}

B32 model_clip_exists(Arena *arena)
{
    B32 res = 0;
    ArenaScoped(arena)
    {
        StringBuilder path = string_init(arena, mscbl_config.model_group.base_dir);
        path_join(&path, mscbl_config.model_group.clip_model.filename);
        res = os_path_exists(StringCast(path));
    }
    return res;
}
