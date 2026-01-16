#include "base/log.h"
#include "base/arena.h"
#include "base/string.h"
#include "base/tree.h"
#include "inference/clip.h"
#include "stb_image.h"
#include "stb_image_resize2.h"
#include "stb_image_write.h"

#include "scan.h"
#include "base/array.h"
#include "base/base_core.h"
#include "os/os_inc.h"
#include "base/threadpool.h"
#include "ui/ui_core.h"
#include "db/db_helpers.h"
#include "inference/clip.cpp"

enum ScanTask
{
    // Step 0
    ScanTask_Nothing,

    // Step 1
    ScanTask_DirScan,

    // Step 2
    ScanTask_AtlasMake,

    // Step 3
    ScanTask_LoadAtlas,

    // Step 4
    ScanTask_EmbedImages,

    ScanTask_COUNT
};

struct ImageEntry
{
    U64 id;
    String path;
    U8 *data;
};

typedef DynamicArray_t(ImageEntry) ImageEntryArray;

struct ScanState
{
    // Dir scanning
    S8 scan_dir_count;

    // State
    U8 step;
    ScanTask last_task;
};

struct ScanAtlasMake
{
    Temp scratch;

    DynamicArray(ImageEntry, entry);
    struct ImageData *img_data;

    U64 num_images;
    U64 group_size;
    U8 *atlas_data;
    // Atlas OP:
    // 0 = Read images into thumbnail array
    // 1 = Draw those images into atlas
    U8 op = 0;
};

struct ScanAtlasLoad
{
    U64 count;
    DynamicArray(ImageEntry, entry);
};

struct ScanEmbed
{
    U64 count;
    clip_ctx *clip;
    DynamicArray(ImageEntry, entry);
    DynamicArray(struct VisionWorker, vision_workers);

    ggml_context *text_ctx;
    ggml_cgraph *text_graph = NULL;
};

ScanState scan_state          = {0};
ScanEmbed scan_embed          = {0};
ScanAtlasMake scan_atlas_make = {0};
ScanAtlasLoad scan_atlas_load = {0};

void scan_restart()
{
    if (scan_state.step > 0)
    {
        // Tasks already running, so write error or something on UI
        return;
    }

    if (!scan_arena)
        scan_arena = arena_alloc(GB(32));
    if (!scan_scratch)
        scan_scratch = arena_alloc(GB(32));
    arena_clear(scan_arena);
    arena_clear(scan_scratch);

    scan_state                = {0};
    scan_atlas_make           = {0};
    scan_atlas_load           = {0};
    scan_state.scan_dir_count = 1;
    scan_atlas_load.count     = scan_load_atlas_count();
    scan_embed.count          = scan_embed_count();
    scan_state.step           = 1;
}

void scan_update()
{
    // Perform cleanup/after tasks (single threaded)
    if (!os_info.pool->busy && scan_state.last_task != ScanTask_Nothing)
    {
        switch (scan_state.last_task)
        {
        case ScanTask_DirScan:
            scan_after_dir();
            break;
        case ScanTask_AtlasMake:
            scan_after_atlas();
            break;
        case ScanTask_LoadAtlas:
            scan_load_atlas_after();
            break;
        case ScanTask_EmbedImages:
            scan_embed.count = 0;
            break;
        default:
            break;
        }
    }

    if ((scan_state.last_task == ScanTask_DirScan && scan_state.scan_dir_count <= 0) ||
        (scan_state.last_task == ScanTask_AtlasMake && scan_atlas_make.num_images <= 0) ||
        (scan_state.last_task == ScanTask_LoadAtlas && scan_atlas_load.count <= 0) ||
        (scan_state.last_task == ScanTask_EmbedImages && scan_embed.count <= 0))
    {
        scan_state.step++;
        scan_state.step %= ScanTask_COUNT;
    }

    // Main task to do (single/multi threaded)
    switch (scan_state.step)
    {
    case 1:
        if (scan_state.scan_dir_count > 0)
        {
            parallel_for(os_info.pool, 1, scan_dirs, NULL);
            scan_state.last_task = ScanTask_DirScan;
            break;
        }
    case 2:
        if (scan_atlas_make.num_images > 0)
        {
            scan_make_atlas();
            scan_state.last_task = ScanTask_AtlasMake;
        }
        break;
    case 3:
        if (scan_atlas_load.count > 0)
        {
            scan_load_atlas();
            scan_state.last_task = ScanTask_LoadAtlas;
        }
        break;
    case 4:
        if (scan_embed.count > 0)
        {
            scan_embed_batch();
            scan_state.last_task = ScanTask_EmbedImages;
        }
        break;
    default:
        scan_state.last_task = ScanTask_Nothing;
        break;
    }
}

/*
 * ***** Dir scanning functions *****
 */

local_v void scan_after_dir()
{
    scan_atlas_make.num_images = scan_atlas_count();
    scan_state.scan_dir_count--;
}

/*
 * ***** Atlas making functions *****
 */

struct ImageData
{
    U8 *data;
    S32 width;
    S32 height;
    S32 channels;
    S32 resize_width;
    S32 resize_height;
};

DB_STMT_CBK(db_push_entry)
{
    ImageEntryArray *entry = (ImageEntryArray *)data;
    ImageEntry e           = {0};
    e.id                   = sqlite3_column_int64(stmt, 0);
    e.path                 = s_cpy(scan_scratch, (const char *)sqlite3_column_text(stmt, 1));
    dyn_array_push(scan_scratch, *entry, e);
}

local_v U64 scan_atlas_count()
{
    U64 count          = 0;
    sqlite3_stmt *stmt = db_prepare("SELECT COUNT(id) FROM Images WHERE atlas_id IS NULL;");
    db_run_stmt(stmt, 1, get_count, &count);

    scan_atlas_make.entry    = dyn_array_init(scan_scratch, count, ImageEntry);
    scan_atlas_make.img_data = push_array(scan_scratch, ATLAS_CAPACITY, ImageData);

    stmt = db_prepare("SELECT id, path FROM Images WHERE atlas_id IS NULL;");
    db_run_stmt(stmt, 1, db_push_entry, &scan_atlas_make.entry);

    // TODO: Try to accomodate left images in a partial atlas
    // stmt = db_prepare("SELECT id, atlas_path, image_count FROM Atlas WHERE image_count < " Stringify(ATLAS_CAPACITY));

    return count;
}

THREAD_FUNC(read_image)
{
    S32 w, h, c;
    S32 resize_height, resize_width;
    U64 entry_idx     = scan_atlas_make.num_images - (1 + n);
    ImageEntry *entry = dyn_array_at(scan_atlas_make.entry, entry_idx);
    U8 *img_data      = stbi_load(str_to_cstr(entry->path), &w, &h, &c, 3);
    if (w < h)
    {
        resize_width  = THUMB_SIZE;
        resize_height = THUMB_SIZE / ((float)w / (float)h);
    }
    else
    {
        resize_height = THUMB_SIZE;
        resize_width  = THUMB_SIZE * ((float)w / (float)h);
    }

    U8 *resize_data = push_size(arena, (3 * resize_height * resize_width), U8);
    stbir_resize_uint8_linear(img_data, w, h, 0, resize_data, resize_width, resize_height, 0, STBIR_RGB);
    stbi_image_free(img_data);
    scan_atlas_make.img_data[n] = {resize_data, w, h, c, resize_width, resize_height};
}

THREAD_FUNC(draw_image)
{
    U32 smaller_side = MIN(scan_atlas_make.img_data[n].resize_width, scan_atlas_make.img_data[n].resize_height);

    U32 x_off = (scan_atlas_make.img_data[n].resize_width - smaller_side) / 2;
    U32 y_off = (scan_atlas_make.img_data[n].resize_height - smaller_side) / 2;

    U8 *src = scan_atlas_make.img_data[n].data + (y_off * scan_atlas_make.img_data[n].resize_width + x_off) * 3;
    U8 *dst = scan_atlas_make.atlas_data + (n / THUMB_PER_SIDE * ATLAS_SIZE * 3 * THUMB_SIZE) +
              (n % THUMB_PER_SIDE * 3 * THUMB_SIZE);

    for (U32 i = 0; i < THUMB_SIZE; i++)
    {
        memcpy(dst, src, THUMB_SIZE * 3);
        src += scan_atlas_make.img_data[n].resize_width * 3;
        dst += ATLAS_SIZE * 3;
    }
}

local_v void scan_make_atlas()
{
    scan_atlas_make.group_size = MIN(ATLAS_CAPACITY, scan_atlas_make.num_images);
    if (scan_atlas_make.op)
    {
        if (parallel_for(os_info.pool, scan_atlas_make.group_size, draw_image, NULL))
            scan_atlas_make.op = 0;
    }
    else
    {
        if (parallel_for(os_info.pool, scan_atlas_make.group_size, read_image, NULL))
        {
            scan_atlas_make.op         = 1;
            scan_atlas_make.scratch    = temp_begin(scan_scratch);
            scan_atlas_make.atlas_data = push_size(scan_scratch, (ATLAS_SIZE * ATLAS_SIZE * 3), U8);
        }
    }
}

local_v void scan_after_atlas()
{
    if (scan_atlas_make.op) return;
    if (!scan_atlas_make.num_images) return;
    char *guid = push_array(scan_scratch, sizeof(Guid) * 2, char);
    bytes_as_hex_lower(os_make_guid().v, 0, sizeof(Guid), guid);
    // TODO: Make config reading system or something to know where to save atlas
    String path = string_format(scan_scratch, ATLAS_DIR "/%.*s.tga", sizeof(Guid) * 2, guid);

    db_run("BEGIN TRANSACTION;");
    U64 atlas_id;
    sqlite3_stmt *stmt;
    stmt = db_prepare("INSERT INTO Atlas(atlas_path, image_count) VALUES(?, ?) RETURNING id;");
    sqlite3_bind_text(stmt, 1, (const char *)path.v, path.size, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, scan_atlas_make.group_size);
    db_run_stmt(stmt, 1, get_count, &atlas_id);

    stmt = db_prepare("UPDATE Images SET atlas_id = ?, atlas_idx = ?, width = ?, height = ?, channels = ? WHERE id = ?;");
    for (U64 i = 0; i < scan_atlas_make.group_size; i++)
    {
        S32 width    = scan_atlas_make.img_data[i].width;
        S32 height   = scan_atlas_make.img_data[i].height;
        S32 channels = scan_atlas_make.img_data[i].channels;
        U64 idx      = scan_atlas_make.num_images - (1 + i);
        U64 id       = dyn_array_at(scan_atlas_make.entry, idx)->id;
        sqlite3_bind_int64(stmt, 1, atlas_id);
        sqlite3_bind_int64(stmt, 2, i);
        sqlite3_bind_int64(stmt, 3, width);
        sqlite3_bind_int64(stmt, 4, height);
        sqlite3_bind_int64(stmt, 5, channels);
        sqlite3_bind_int64(stmt, 6, id);
        db_run_stmt(stmt, 0);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        Image_Node *img = tree_find(&ui_state.images, (Image *)&id, Image_cmp, Image);
        Assert(img);
        img->v.atlas_id  = atlas_id;
        img->v.atlas_idx = i;
    }
    sqlite3_finalize(stmt);
    db_run("COMMIT;");

    stbi_write_tga(str_to_cstr(path), ATLAS_SIZE, ATLAS_SIZE, 3, scan_atlas_make.atlas_data);
    mscbl_log_dbg(scan_atlas_make, "Written atlas: %.*s\n", path.size, str_to_cstr(path));

    U32 *tex_id;
    Atlas_Node *res = tree_find(&ui_state.atlas, (Atlas *)&atlas_id, Atlas_cmp, Atlas);
    if (res)
    {
        tex_id = &res->v.tex;
    }
    else
    {
        Atlas a       = {atlas_id, NULL, 0, 1};
        Atlas_Node *n = tree_node(mscbl.persistent_arena, a, Atlas);
        tree_push(&ui_state.atlas, n, Atlas_cmp, Atlas);
        tex_id = &n->v.tex;
    }

    gl_make_texture(tex_id, scan_atlas_make.atlas_data, ATLAS_SIZE, ATLAS_SIZE, 3);

    arena_array_clear(os_info.pool->worker_arena);
    temp_end(scan_atlas_make.scratch);
    scan_atlas_make.atlas_data = NULL;
    scan_atlas_make.num_images -= scan_atlas_make.group_size;
}

/*
 * ***** Atlas loading functions *****
 */

THREAD_FUNC(atlas_load)
{
    ImageEntry *v = dyn_array_at(scan_atlas_load.entry, n);
    Atlas_Node *p = tree_find(&ui_state.atlas, (Atlas *)&v->id, Atlas_cmp, Atlas);
    if (p)
    {
        mscbl_log_dbg(atlas_load, "Skipped loading %.*s\n", v->path.size, v->path.v);
        return;
    }

    S32 w, h;
    dyn_array_at(scan_atlas_load.entry, n)->data = stbi_load(str_to_cstr(v->path), &w, &h, NULL, 3);
}

local_v U64 scan_load_atlas_count()
{
    U64 count          = 0;
    sqlite3_stmt *stmt = db_prepare("SELECT COUNT(id) from Atlas;");
    db_run_stmt(stmt, 1, get_count, &count);
    scan_atlas_load.entry = dyn_array_init(scan_scratch, count, ImageEntry);
    return count;
}

local_v void scan_load_atlas()
{
    if (scan_atlas_load.entry.size < scan_atlas_load.count)
    {
        sqlite3_stmt *stmt = db_prepare("SELECT id, atlas_path from Atlas;");
        db_run_stmt(stmt, 1, db_push_entry, &scan_atlas_load.entry);
    }

    parallel_for(os_info.pool, scan_atlas_load.count, atlas_load, NULL);
}

local_v void scan_load_atlas_after()
{
    for (U64 i = 0; i < scan_atlas_load.entry.size; i++)
    {
        ImageEntry *entry = dyn_array_at(scan_atlas_load.entry, i);
        Atlas atl         = {entry->id, entry->data, 0, 1};
        Atlas_Node *n     = tree_node(mscbl.persistent_arena, atl, Atlas);
        gl_make_texture(&n->v.tex, entry->data, ATLAS_SIZE, ATLAS_SIZE, 3);
        tree_push(&ui_state.atlas, n, Atlas_cmp, Atlas);
        stbi_image_free(entry->data);
    }
    scan_atlas_load.count -= scan_atlas_load.entry.size;
    async_job(os_info.pool, ui_reload_order, NULL);
}

/*
 * ***** Image embedding functions *****
 */

#define MODEL_BATCH_SIZE 8

local_v U64 scan_embed_count()
{
    U64 count          = 0;
    sqlite3_stmt *stmt = db_prepare("SELECT COUNT(id) FROM Images WHERE embedding IS NULL;");
    db_run_stmt(stmt, 1, get_count, &count);

    scan_embed.entry = dyn_array_init(scan_scratch, count, ImageEntry);
    stmt             = db_prepare("SELECT id, path FROM Images WHERE embedding IS NULL;");
    db_run_stmt(stmt, 1, db_push_entry, &scan_embed.entry);

    mscbl_log_dbg(SCAN, "Can embed %zu images\n", count);
    return count;
}

F32 *preprocess_batch_from_to(Arena *arena, ImageEntry *entries, U64 size, U64 from, U64 to)
{
    F32 *data;
    S32 w, h;

    U32 frame_size  = size * size;
    U32 image_size  = frame_size * 3;
    F32 *resized    = push_array(arena, image_size, F32);
    F32 *batch_data = push_array0(arena, (image_size * MODEL_BATCH_SIZE), F32);
    F32 *mean       = scan_embed.clip->image_mean;
    F32 inv_std[3]  = {
        1 / scan_embed.clip->image_std[0],
        1 / scan_embed.clip->image_std[1],
        1 / scan_embed.clip->image_std[2],
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

// FIXME: This can only run in 1 thread concurrently. GGML makes it's own threads
// so, there's no need to actually make more than thread.
THREAD_FUNC(embed_batch)
{
    mscbl_log_dbg(embed_batch, "Arena usage: %zu bytes (%.4f)\n", arena->used, (float)(arena->used) / (arena->capacity));

    VisionWorker *worker = dyn_array_at(scan_embed.vision_workers, id);
    ggml_context **ctx   = &worker->ctx;
    ggml_cgraph **graph  = &worker->graph;

    if (worker->valid == Vision_None)
    {
        U64 mem_size = (ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead());
        *ctx         = ggml_init({mem_size, arena_push(arena, mem_size, 0, GGML_MEM_ALIGN), true});
        *graph       = build_image_encode_graph(*ctx, scan_embed.clip, MODEL_BATCH_SIZE);
        mscbl_log_dbg(embed_batch, "Graph context mem usage: %zu/%zu (%.6f)\n",
                      ggml_used_mem(*ctx), ggml_get_mem_size(*ctx),
                      (float)ggml_used_mem(*ctx) / (float)ggml_get_mem_size(*ctx));
        worker->valid = Vision_GraphInit;
    }

    Temp scratch = temp_begin(arena);

    U64 base        = MODEL_BATCH_SIZE * n;
    U64 end         = MIN(base + MODEL_BATCH_SIZE, scan_embed.count);
    U64 size        = clip_get_vision_hparams(scan_embed.clip)->image_size;
    F32 *image_data = preprocess_batch_from_to(arena, scan_embed.entry.v, size, base, end);
    // stbi_write_hdr("1.hdr", 224, 224 * 3 * MODEL_BATCH_SIZE, 1, image_data);

    // Create embeddings
    // ggml_init_params graph_params = {graph_size, NULL, true};
    // ggml_context *vision_ctx	  = ggml_init(graph_params);
    // ggml_cgraph *vision_graph	  = build_image_encode_graph(vision_ctx, &clip_data.clip, (end - base));
    F32 *embeddings    = clip_get_image_embedding(arena, scan_embed.clip, worker, image_data, MODEL_BATCH_SIZE);
    S32 embedding_size = clip_get_vision_hparams(scan_embed.clip)->projection_dim;

    sqlite3_stmt *stmt = db_prepare("UPDATE Images SET embedding = ? WHERE id = ?;");
    for (U64 i = 0; i < (end - base); i++)
    {
        sqlite3_bind_blob(stmt, 1, embeddings + i * embedding_size, embedding_size * sizeof(F32), SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, dyn_array_at(scan_embed.entry, base + i)->id);
        db_run_stmt(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);

    temp_end(scratch);
}

local_v void scan_embed_batch()
{
    if (!scan_embed.clip)
    {
        scan_embed.clip = push_struct(mscbl.persistent_arena, clip_ctx);
        clip_model_load(mscbl.persistent_arena, scan_embed.clip, MODEL_PATH);
    }
    U64 task_count   = ToCeilInt(scan_embed.count, MODEL_BATCH_SIZE);
    U64 worker_count = MIN(os_info.pool->worker_count, task_count);

    if (!scan_embed.vision_workers.size)
    {
        scan_embed.vision_workers = dyn_array_init(scan_scratch, worker_count, VisionWorker);
        for (U64 i = 0; i < worker_count; i++)
        {
            VisionWorker v = {0};
            dyn_array_push(scan_scratch, scan_embed.vision_workers, v);
        }
    }

    parallel_for(os_info.pool, task_count, embed_batch, NULL);
}
