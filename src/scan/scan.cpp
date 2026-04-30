#include "base/log.h"
#include "config.h"
#include "base/arena.h"
#include "base/string.h"
#include "base/tree.h"
// #include "index/index.h"
#include "inference/clip.h"
#include "inference/model.h"
#include "stb_image.h"
#include "stb_image_resize2.h"
#include "stb_image_write.h"
#include "cli_args.h"
#include "gl/gl_core.h"

#include "scan.h"
#include "scan_dirs.cpp"
#include "scan/scan_atlas.cpp"
#include "base/array.h"
#include "base/base_core.h"
#include "os/os_inc.h"
#include "base/threadpool.h"
#include "ui/ui_core.h"
#include "db/db_helpers.h"
#include "inference/clip.cpp"
#include "inference/model.cpp"

enum ScanTask
{
    // Step 0
    ScanTask_Nothing,

    // Step 2
    ScanTask_AtlasMake,

    // Step 3
    ScanTask_LoadAtlas,

    // Step 4
    ScanTask_EmbedImages,

    ScanTask_COUNT
};

enum TaskStatus
{
    Status_NotDoing,
    Status_Doing,
    Status_Done,
};

Arena *scan_arena = NULL;

// struct ScanState
// {
//     // Dir scanning
//     S8 scan_dir_count;
//
//     // State
//     U8 step;
//     ScanTask last_task;
// };

// struct ScanAtlasMake
// {
//     Temp scratch;
//
//     ImageEntry *entry;
//     struct ImageData *img_data;
//
//     U64 num_images;
//     U64 group_size;
//     U8 *atlas_data;
//     // Atlas OP:
//     // 0 = Read images into thumbnail array
//     // 1 = Draw those images into atlas
//     U8 op = 0;
// };
//
// struct ScanAtlasLoad
// {
//     S64 count;
//     ImageEntry *entry;
// };
//
// struct ScanEmbed
// {
//     S64 count;
//     ImageEntry *entry;
//     struct VisionWorker *vision_workers;
// };
//
// ScanState scan_state          = {0};
// ScanEmbed scan_embed          = {0};
// ScanAtlasMake scan_atlas_make = {0};
// ScanAtlasLoad scan_atlas_load = {0};
//
// void scan_restart()
// {
//     if (scan_state.step > 0)
//     {
//         // Tasks already running, so write error or something on UI
//         return;
//     }
//
//     if (!scan_arena)
//         arena_alloc(GB(32), scan_arena);
//     if (!scan_scratch)
//         arena_alloc(GB(32), scan_scratch);
//     if (!atlas_arena)
//         arena_alloc(MB(1), atlas_arena);
//     arena_clear(scan_arena);
//     arena_clear(scan_scratch);
//
//     scan_state                = {0};
//     scan_atlas_make           = {0};
//     scan_atlas_load           = {0};
//     scan_state.scan_dir_count = 1;
//     scan_atlas_load.count     = scan_load_atlas_count();
//     scan_embed.count          = scan_embed_count();
//     scan_state.step           = 1;
//
//     if (cli_args.noembed)
//         scan_embed.count = 0;
// }
//
// void scan_update()
// {
//     // Perform cleanup/after tasks (single threaded)
//     // if (!os_info.pool->busy && scan_state.last_task != ScanTask_Nothing)
//     {
//         switch (scan_state.last_task)
//         {
//         case ScanTask_DirScan:
//             scan_after_dir();
//             break;
//         case ScanTask_AtlasMake:
//             scan_after_atlas();
//             break;
//         case ScanTask_LoadAtlas:
//             scan_load_atlas_after();
//             break;
//         case ScanTask_EmbedImages:
//             scan_embed.count = 0;
//             break;
//         default:
//             break;
//         }
//     }
//
//     if ((scan_state.last_task == ScanTask_DirScan && scan_state.scan_dir_count <= 0) ||
//         (scan_state.last_task == ScanTask_AtlasMake && scan_atlas_make.num_images <= 0) ||
//         (scan_state.last_task == ScanTask_LoadAtlas && scan_atlas_load.count <= 0) ||
//         (scan_state.last_task == ScanTask_EmbedImages && scan_embed.count <= 0))
//     {
//         scan_state.step++;
//         scan_state.step %= ScanTask_COUNT;
//     }
//
//     // Main task to do (single/multi threaded)
//     switch (scan_state.step)
//     {
//     case 1:
//         if (scan_state.scan_dir_count > 0)
//         {
//             // parallel_for(os_info.pool, 1, scan_dirs, NULL);
//             scan_state.last_task = ScanTask_DirScan;
//             break;
//         }
//     case 2:
//         if (scan_atlas_make.num_images > 0)
//         {
//             scan_make_atlas();
//             scan_state.last_task = ScanTask_AtlasMake;
//         }
//         break;
//     case 3:
//         if (scan_atlas_load.count > 0)
//         {
//             scan_load_atlas();
//             scan_state.last_task = ScanTask_LoadAtlas;
//         }
//         break;
//     case 4:
//         if (scan_embed.count > 0)
//         {
//             scan_embed_batch();
//             scan_state.last_task = ScanTask_EmbedImages;
//         }
//         break;
//     default:
//         scan_state.last_task = ScanTask_Nothing;
//         break;
//     }
// }
//
// /*
//  * ***** Dir scanning functions *****
//  */
//
// local_v void scan_after_dir()
// {
//     // async_job(os_info.pool, index_fill, NULL);
//     scan_atlas_make.num_images = scan_atlas_count();
//     scan_state.scan_dir_count--;
// }
//
// /*
//  * ***** Atlas making functions *****
//  */
//
// struct ImageData
// {
//     U8 *data;
//     S32 width;
//     S32 height;
//     S32 channels;
//     S32 resize_width;
//     S32 resize_height;
// };
//
// DBStmtCbk(db_push_entry)
// {
//     ImageEntry *entry = (ImageEntry *)data;
//     ImageEntry e      = {0};
//     e.id              = sqlite3_column_int64(stmt, 0);
//     e.path            = string_cpy(scan_scratch, sqlite3_column_text(stmt, 1));
//     da_push(scan_scratch, entry, e, ImageEntry);
// }
//
// local_v U64 scan_atlas_count()
// {
//     U64 count          = 0;
//     sqlite3_stmt *stmt = db_prepare("SELECT COUNT(id) FROM Images WHERE atlas_id IS NULL;");
//     db_run_stmt(stmt, 1, get_count, &count);
//
//     da_setcap(scan_scratch, scan_atlas_make.entry, count, ImageEntry);
//     scan_atlas_make.img_data = push_array(scan_scratch, ATLAS_CAPACITY, ImageData);
//
//     stmt = db_prepare("SELECT id, path FROM Images WHERE atlas_id IS NULL;");
//     db_run_stmt(stmt, 1, db_push_entry, scan_atlas_make.entry);
//
//     // TODO: Try to accomodate left images in a partial atlas
//     // stmt = db_prepare("SELECT id, atlas_path, image_count FROM Atlas WHERE image_count < " Stringify(ATLAS_CAPACITY));
//
//     return count;
// }
//
//
//
// local_v void scan_make_atlas()
// {
//     scan_atlas_make.group_size = MIN(ATLAS_CAPACITY, scan_atlas_make.num_images);
//     if (scan_atlas_make.op)
//     {
//         // if (parallel_for(os_info.pool, scan_atlas_make.group_size, draw_image, NULL))
//         scan_atlas_make.op = 0;
//     }
//     else
//     {
//         // if (parallel_for(os_info.pool, scan_atlas_make.group_size, read_image, NULL))
//         {
//             scan_atlas_make.op         = 1;
//             scan_atlas_make.scratch    = temp_begin(scan_scratch);
//             scan_atlas_make.atlas_data = push_size(scan_scratch, (ATLAS_SIZE * ATLAS_SIZE * 3), U8);
//         }
//     }
// }
//
// local_v void scan_after_atlas()
// {
//     if (scan_atlas_make.op) return;
//     if (!scan_atlas_make.num_images) return;
//     char *guid = push_array(scan_scratch, sizeof(Guid) * 2, char);
//     bytes_as_hex_lower(os_make_guid().v, 0, sizeof(Guid), guid);
//     // TODO: Make config reading system or something to know where to save atlas
//     StringBuilder path = string_empty(scan_scratch);
//     string_format(&path, "%.*s/%.*s.tga", mscbl_config.atlas_dir.size, mscbl_config.atlas_dir.v, sizeof(Guid) * 2, guid);
//
//     db_run("BEGIN TRANSACTION;");
//     U64 atlas_id;
//     sqlite3_stmt *stmt;
//     stmt = db_prepare("INSERT INTO Atlas(atlas_path, image_count) VALUES(?, ?) RETURNING id;");
//     sqlite3_bind_text(stmt, 1, (const char *)path.v, path.size, SQLITE_STATIC);
//     sqlite3_bind_int64(stmt, 2, scan_atlas_make.group_size);
//     db_run_stmt(stmt, 1, get_count, &atlas_id);
//
//     stmt = db_prepare("UPDATE Images SET atlas_id = ?, atlas_idx = ?, width = ?, height = ?, channels = ? WHERE id = ?;");
//     for (U64 i = 0; i < scan_atlas_make.group_size; i++)
//     {
//         S32 width    = scan_atlas_make.img_data[i].width;
//         S32 height   = scan_atlas_make.img_data[i].height;
//         S32 channels = scan_atlas_make.img_data[i].channels;
//         U64 idx      = scan_atlas_make.num_images - (1 + i);
//         U64 id       = scan_atlas_make.entry[idx].id;
//         sqlite3_bind_int64(stmt, 1, atlas_id);
//         sqlite3_bind_int64(stmt, 2, i);
//         sqlite3_bind_int64(stmt, 3, width);
//         sqlite3_bind_int64(stmt, 4, height);
//         sqlite3_bind_int64(stmt, 5, channels);
//         sqlite3_bind_int64(stmt, 6, id);
//         db_run_stmt(stmt, 0);
//         sqlite3_reset(stmt);
//         sqlite3_clear_bindings(stmt);
//
//         // Image_Node *img = tree_find(&ui_state.images, (Image *)&id, Image_cmp, Image);
//         // Assert(img);
//         // img->v.atlas_id  = atlas_id;
//         // img->v.atlas_idx = i;
//         //
//         // img->v.width    = width;
//         // img->v.height   = height;
//         // img->v.channels = channels;
//     }
//     sqlite3_finalize(stmt);
//     db_run("COMMIT;");
//
//     Assert(stbi_write_tga(CStrCast(path), ATLAS_SIZE, ATLAS_SIZE, 3, scan_atlas_make.atlas_data), "failed to save image (%.*s)", path.size, path.v);
//     mscbl_log_dbg("Written atlas: %.*s", path.size, CStrCast(path));
//
//     U32 *tex_id;
//     Atlas_Node *res = tree_find(&ui_state.atlas, (Atlas *)&atlas_id, Atlas_cmp, Atlas);
//     if (res)
//     {
//         tex_id = &res->v.tex;
//     }
//     else
//     {
//         Atlas a       = {atlas_id, NULL, 0, 1};
//         Atlas_Node *n = tree_node(atlas_arena, a, Atlas);
//         tree_push(&ui_state.atlas, n, Atlas_cmp, Atlas);
//         tex_id = &n->v.tex;
//     }
//
//     gl_make_texture(tex_id, scan_atlas_make.atlas_data, ATLAS_SIZE, ATLAS_SIZE, 3);
//
//     arena_array_clear(os_info.pool->worker_arena);
//     temp_end(scan_atlas_make.scratch);
//     scan_atlas_make.atlas_data = NULL;
//     scan_atlas_make.num_images -= scan_atlas_make.group_size;
//     // async_job(os_info.pool, index_fill, NULL);
// }
//
// /*
//  * ***** Atlas loading functions *****
//  */
//
// local_v U64 scan_load_atlas_count()
// {
//     U64 count          = 0;
//     sqlite3_stmt *stmt = db_prepare("SELECT COUNT(id) from Atlas;");
//     db_run_stmt(stmt, 1, get_count, &count);
//     da_setcap(scan_scratch, scan_atlas_load.entry, count, ImageEntry);
//     return count;
// }
//
// ThreadFunc(atlas_load)
// {
//     // ImageEntry v  = scan_atlas_load.entry[n];
//     // Atlas_Node *p = tree_find(&ui_state.atlas, (Atlas *)&v.id, Atlas_cmp, Atlas);
//     // if (p)
//     // {
//     //     mscbl_log_dbg("Skipped loading %.*s\n", v.path.size, v.path.v);
//     //     return;
//     // }
//     //
//     // S32 w, h;
//     // U8 *img_data = stbi_load(CStrCast(v.path), &w, &h, NULL, 3);
//     // Assert(img_data, "image data is NULL (%.*s)", v.path.size, v.path.v);
//     // scan_atlas_load.entry[n].data = img_data;
// }
//
// local_v void scan_load_atlas()
// {
//     if (da_getsize(scan_atlas_load.entry) < (U64)scan_atlas_load.count)
//     {
//         sqlite3_stmt *stmt = db_prepare("SELECT id, atlas_path from Atlas;");
//         db_run_stmt(stmt, 1, db_push_entry, scan_atlas_load.entry);
//     }
//
//     // parallel_for(os_info.pool, scan_atlas_load.count, atlas_load, NULL);
// }
//
// local_v void scan_load_atlas_after()
// {
//     if (scan_atlas_load.count <= 0)
//         return;
//     while (scan_atlas_load.count--)
//     {
//         ImageEntry entry = scan_atlas_load.entry[scan_atlas_load.count];
//         Atlas atl        = {entry.id, 0, 0, 1};
//         Atlas_Node *n    = tree_node(atlas_arena, atl, Atlas);
//         gl_make_texture(&n->v.tex, entry.data, ATLAS_SIZE, ATLAS_SIZE, 3);
//         tree_push(&ui_state.atlas, n, Atlas_cmp, Atlas);
//         stbi_image_free(entry.data);
//     }
// }
//
// /*
//  * ***** Image embedding functions *****
//  */
//
// #define MODEL_BATCH_SIZE 8
//
// local_v U64 scan_embed_count()
// {
//     U64 count          = 0;
//     sqlite3_stmt *stmt = db_prepare("SELECT COUNT(id) FROM Images WHERE embedding IS NULL;");
//     db_run_stmt(stmt, 1, get_count, &count);
//
//     da_setcap(scan_scratch, scan_embed.entry, count, ImageEntry);
//     stmt = db_prepare("SELECT id, path FROM Images WHERE embedding IS NULL;");
//     db_run_stmt(stmt, 1, db_push_entry, &scan_embed.entry);
//
//     mscbl_log_dbg("Can embed %zu images", count);
//     return count;
// }
//
// F32 *preprocess_batch_from_to(Arena *arena, ImageEntry *entries, U64 size, U64 from, U64 to)
// {
//     F32 *data;
//     S32 w, h;
//
//     U32 frame_size  = size * size;
//     U32 image_size  = frame_size * 3;
//     F32 *resized    = push_array(arena, image_size, F32);
//     F32 *batch_data = push_array0(arena, (image_size * MODEL_BATCH_SIZE), F32);
//     F32 *mean       = model.clip->image_mean;
//     F32 inv_std[3]  = {
//         1 / model.clip->image_std[0],
//         1 / model.clip->image_std[1],
//         1 / model.clip->image_std[2],
//     };
//
//     U8 batch_idx = 0;
//     for (U64 idx = from; idx < to; idx++)
//     {
//         data = stbi_loadf(CStrCast(entries[idx].path), &w, &h, NULL, 3);
//         Assert(data, "image data is NULL (%.*s)", entries[idx].path.size, entries[idx].path.v);
//         stbir_resize_float_linear(data, w, h, 0, resized, size, size, 0, STBIR_RGB);
//
//         // TODO: Please find something to speed it up. This abomination takes ~4s per image to process
//         for (U32 i = 0; i < frame_size; i++)
//         {
//             batch_data[batch_idx * image_size + 0 * frame_size + i] = ((resized[3 * i + 0] - mean[0]) * inv_std[0]); /* red */
//             batch_data[batch_idx * image_size + 1 * frame_size + i] = ((resized[3 * i + 1] - mean[1]) * inv_std[1]); /* green */
//             batch_data[batch_idx * image_size + 2 * frame_size + i] = ((resized[3 * i + 2] - mean[2]) * inv_std[2]); /* blue */
//         }
//
//         stbi_image_free(data);
//         batch_idx++;
//     }
//     if (batch_idx < MODEL_BATCH_SIZE) MemoryZero(batch_data + (batch_idx * image_size), (MODEL_BATCH_SIZE - batch_idx) * image_size * sizeof(F32));
//
//     return batch_data;
// }
//
// // FIXME: This can only run in 1 thread concurrently. GGML makes it's own threads
// // so, there's no need to actually make more than thread.
// ThreadFunc(embed_batch)
// {
//     // mscbl_log_dbg("Arena usage: %zu bytes (%.4f)\n", arena->used, (float)(arena->used) / (arena->capacity));
//     //
//     // VisionWorker worker = scan_embed.vision_workers[id];
//     // ggml_context **ctx  = &worker.ctx;
//     // ggml_cgraph **graph = &worker.graph;
//     //
//     // if (worker.valid == Vision_None)
//     // {
//     //     U64 mem_size = (ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead());
//     //     *ctx         = ggml_init({mem_size, arena_push(arena, mem_size, 0, GGML_MEM_ALIGN), true});
//     //     *graph       = build_image_encode_graph(*ctx, model.clip, MODEL_BATCH_SIZE);
//     //     mscbl_log_dbg("Graph context mem usage: %zu/%zu (%.6f)\n",
//     //                   ggml_used_mem(*ctx), ggml_get_mem_size(*ctx),
//     //                   (float)ggml_used_mem(*ctx) / (float)ggml_get_mem_size(*ctx));
//     //     worker.valid = Vision_GraphInit;
//     // }
//     //
//     // Temp scratch = temp_begin(arena);
//     //
//     // U64 base           = MODEL_BATCH_SIZE * n;
//     // U64 end            = MIN(base + MODEL_BATCH_SIZE, (U64)scan_embed.count);
//     // U64 size           = clip_get_vision_hparams(model.clip)->image_size;
//     // F32 *image_data    = preprocess_batch_from_to(arena, scan_embed.entry, size, base, end);
//     // F32 *embeddings    = clip_get_image_embedding(arena, model.clip, &worker, image_data, MODEL_BATCH_SIZE);
//     // S32 embedding_size = clip_get_vision_hparams(model.clip)->projection_dim;
//     //
//     // sqlite3_stmt *stmt = db_prepare("UPDATE Images SET embedding = ? WHERE id = ?;");
//     // for (U64 i = 0; i < (end - base); i++)
//     // {
//     //     sqlite3_bind_blob(stmt, 1, embeddings + i * embedding_size, embedding_size * sizeof(F32), SQLITE_STATIC);
//     //     sqlite3_bind_int64(stmt, 2, scan_embed.entry[base + i].id);
//     //     db_run_stmt(stmt);
//     //     sqlite3_reset(stmt);
//     //     sqlite3_clear_bindings(stmt);
//     // }
//     // sqlite3_finalize(stmt);
//     //
//     // temp_end(scratch);
// }
//
// local_v void scan_embed_batch()
// {
//     if (!model_arena)
//         arena_alloc(MB(100), model_arena);
//     if (!model.clip)
//     {
//         model.clip = push_struct(model_arena, clip_ctx);
//         clip_model_load(model_arena, model.clip, MODEL_PATH);
//     }
//     U64 task_count   = ToCeilInt(scan_embed.count, MODEL_BATCH_SIZE);
//     U64 worker_count = MIN(os_info.pool->worker_count, task_count);
//
//     if (!scan_embed.vision_workers)
//     {
//         da_setcap(scan_scratch, scan_embed.vision_workers, worker_count, VisionWorker);
//         for (U64 i = 0; i < worker_count; i++)
//         {
//             VisionWorker v = {0};
//             da_push(scan_scratch, scan_embed.vision_workers, v, VisionWorker);
//         }
//     }
//
//     // parallel_for(os_info.pool, task_count, embed_batch, NULL);
// }
