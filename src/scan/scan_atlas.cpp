#include "base/arena.h"
#include "gl/gl_core.h"
#include "base/base_core.h"
#include "base/string.h"
#include "base/threadpool.h"
#include "config.h"
#include "db/db_helpers.h"
#include "db/fetch.h"
#include "gl/gl_core.h"
#include "os/os_inc.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"

#include "base/log.h"
#include "base/array.h"
#include "scan/scan.h"

global_v U8 *atlas_data = NULL;

struct read_params
{
    ImageRow *row;
    ArenaArray arena_array;
};

ThreadFunc(read_image)
{
    Assert(data.kind == TPData_ANY, "wrong datatype");
    read_params *params0 = (read_params *)data.val_any;
    ImageRow *row = params0->row;
    Arena *read_arena = params0->arena_array.v[id];

    S32 w, h, c;
    S32 resize_height, resize_width;
    U8 *img_data = stbi_load(CStrCast(row->path), &w, &h, &c, ATLAS_CHANNELS);
    Assert(img_data, "image data is NULL (%.*s)", StringSpr(row->path));

    if (w < h)
    {
        resize_width = THUMB_SIZE;
        resize_height = THUMB_SIZE / ((F32)w / (F32)h);
    }
    else
    {
        resize_height = THUMB_SIZE;
        resize_width = THUMB_SIZE * ((F32)w / (F32)h);
    }

    U8 *resize_data = push_size(read_arena, (ATLAS_CHANNELS * resize_height * resize_width), U8);
    stbir_resize_uint8_linear(img_data, w, h, 0, resize_data, resize_width, resize_height, 0, STBIR_RGB);
    stbi_image_free(img_data);

    row->width = w;
    row->height = h;
    row->channels = c;
    row->data = resize_data;
    row->resize_width = resize_width;
    row->resize_height = resize_height;
}

struct draw_params
{
    S64 draw_idx;
    ImageRow *row;
};

ThreadFunc(draw_image)
{
    Assert(data.kind == TPData_ANY, "wrong datatype");
    draw_params params = *(draw_params *)data.val_any;
    U32 smaller_side = MIN(params.row->resize_width, params.row->resize_height);

    U32 x_off = (params.row->resize_width - smaller_side) / 2;
    U32 y_off = (params.row->resize_height - smaller_side) / 2;

    U8 *src = params.row->data + (y_off * params.row->resize_width + x_off) * ATLAS_CHANNELS;
    U8 *dst = atlas_data + (params.draw_idx / THUMB_PER_SIDE * ATLAS_SIZE * ATLAS_CHANNELS * THUMB_SIZE) +
              (params.draw_idx % THUMB_PER_SIDE * ATLAS_CHANNELS * THUMB_SIZE);

    for (U32 i = 0; i < THUMB_SIZE; i++)
    {
        memcpy(dst, src, THUMB_SIZE * ATLAS_CHANNELS);
        src += params.row->resize_width * ATLAS_CHANNELS;
        dst += ATLAS_SIZE * ATLAS_CHANNELS;
    }
}

struct AtlasRow
{
    S64 atlas_id;
    String path;
    B32 update;
};

DBStmtCbk(get_atlas)
{
    AtlasRow *ret = (AtlasRow *)data;
    AtlasRow row = {
        .atlas_id = sqlite3_column_int64(stmt, 0),
        .path = string_copy(arena, sqlite3_column_text(stmt, 1)),
        .update = 1};
    *ret = row;
}

DBStmtCbk(push_idx)
{
    draw_params **array = (draw_params **)data;
    draw_params idx = {
        .draw_idx = sqlite3_column_int64(stmt, 0)};
    da_push(arena, *array, idx);
}

void scan_atlas_bake(Arena *arena, ImageRow *inserted)
{
    U64 inserted_count = da_getsize(inserted);

    Semaphore batch_sem = os_semaphore_alloc(0, S32_MAX);

    // NOTE: setting a memory arena (per worker) for reading images into
    // worker arena memory is only valid till one function call and can't be
    // relied upon for usage across multiple worker cycles
    U64 worker_count = threadpool_worker_count();
    ArenaArray worker_memory = arena_array_alloc(MB(50), worker_count);

    for (U64 base = 0; base < inserted_count; base += ATLAS_CAPACITY)
    {
        ArenaScoped(arena)
        {
            arena_array_clear(worker_memory);

            AtlasRow row = {0};
            read_params *read_prm = NULL;
            draw_params *draw_prm = NULL;
            da_setcap(arena, draw_prm, ATLAS_CAPACITY);
            da_setcap(arena, read_prm, ATLAS_CAPACITY);

            // NOTE: get number of images to be drawn (0, 100]
            S64 task_count = MIN(base + ATLAS_CAPACITY, inserted_count) - base;
            S64 batch_size = task_count;
            for (S64 off = 0; off < task_count; off++)
            {
                // NOTE: read jobs are taken from entire inserted array
                // so job index = base + off (offset of current job)
                read_params params = {
                    .row = &inserted[off + base],
                    .arena_array = worker_memory};
                da_push(arena, read_prm, params);
                AsyncTask task = {
                    .func = read_image,
                    .data = {
                        .kind = TPData_ANY,
                        .val_any = &read_prm[off]},
                    .batch_size = &batch_size,
                    .batch_complete = batch_sem};
                threadpool_enqueue(TaskPriority_High, task);
            }

            // NOTE: find the existing atlas which can accomodate the images of this batch
            sqlite3_stmt *st0 = db_prepare(
                "SELECT s.atlas_id, a.atlas_path "
                "FROM AtlasSlots s JOIN Atlas a "
                "ON s.atlas_id = a.id "
                "WHERE s.atlas_id IN "
                "(SELECT atlas_id "
                "FROM AtlasSlots "
                "WHERE image_id IS NULL "
                "GROUP BY atlas_id "
                "HAVING COUNT(*) >= ?);");
            sqlite3_bind_int64(st0, 1, inserted_count - base);

            if (db_run_stmt(st0, 1, get_atlas, &row, arena))
            {
                // NOTE: get the available slot indices from the existing atlas
                sqlite3_stmt *st1 = db_prepare(
                    "SELECT atlas_idx "
                    "FROM AtlasSlots "
                    "WHERE atlas_id = ? "
                    "AND image_id IS NULL;");
                sqlite3_bind_int64(st1, 1, row.atlas_id);
                db_run_stmt(st1, 1, push_idx, &draw_prm, arena);

                // NOTE: load the existing atlas for drawing
                S32 w, h, c;
                atlas_data = stbi_load(CStrCast(row.path), &w, &h, &c, ATLAS_CHANNELS);
            }

            if (!da_getsize(draw_prm))
            {
                // NOTE: if no existing atlas has enough slots available, create new
                // atlas with new guid and path
                char *guid = push_array(arena, sizeof(Guid) * 2, char);
                bytes_as_hex_lower(os_make_guid().v, 0, sizeof(Guid), guid);

                StringBuilder path = string_empty(arena);
                string_format(&path, "%.*s/%.*s.tga", StringSpr(mscbl_config.atlas_dir), sizeof(Guid) * 2, guid);

                row = {.atlas_id = -1,
                       .path = StringCast(path),
                       .update = 0};

                for (S64 j = 0; j < task_count; j++)
                {
                    // NOTE: create draw command list
                    draw_params pos = {.draw_idx = j};
                    da_push(arena, draw_prm, pos);
                }

                atlas_data = push_array(arena, (ATLAS_SIZE * ATLAS_SIZE * ATLAS_CHANNELS), U8);
            }

            // NOTE: wait until batch of read_image commands finishes
            os_semaphore_take(batch_sem, U64_MAX);

            batch_size = task_count;
            for (S64 off = 0; off < task_count; off++)
            {
                // NOTE: draw jobs reads rows from base + off (offset of job) and
                // draws them into draw_idx (<=ATLAS_CAPACITY) which is taken from
                // off (current offset) index of draw_pos array
                draw_prm[off].row = &inserted[off + base];
                AsyncTask task = {
                    .func = draw_image,
                    .data = {
                        .kind = TPData_ANY,
                        .val_any = &draw_prm[off]},
                    .batch_size = &batch_size,
                    .batch_complete = batch_sem};
                threadpool_enqueue(TaskPriority_High, task);
            }

            // NOTE: save the atlas and update the database as 1 transaction
            db_run("BEGIN TRANSACTION;");

            // NOTE: insert atlas if new and obtain it's atlas_id
            if (!row.update)
            {
                sqlite3_stmt *st2 = db_prepare("INSERT INTO Atlas(atlas_path) VALUES(?) RETURNING id;");
                sqlite3_bind_text(st2, 1, CStrCast(row.path), row.path.size, SQLITE_STATIC);
                db_run_stmt(st2, 1, get_id, &row.atlas_id);
            }

            //NOTE: insert images and Create slot entries
            sqlite3_stmt *insert_stmt = db_prepare("UPDATE Images SET atlas_id = ?, atlas_idx = ?, width = ?, height = ?, channels = ? WHERE id = ?;");
            sqlite3_stmt *slot_stmt = db_prepare("INSERT INTO AtlasSlots (atlas_id, atlas_idx, image_id) VALUES (?, ?, ?) ON CONFLICT(atlas_id, atlas_idx) DO UPDATE SET image_id = excluded.image_id;");
            for (S64 off = 0; off < task_count; off++)
            {
                S64 image_id = inserted[off + base].id;
                S64 atlas_id = row.atlas_id;
                U32 atlas_idx = draw_prm[off].draw_idx;
                S32 width = inserted[off + base].width;
                S32 height = inserted[off + base].height;
                S32 channels = inserted[off + base].channels;

                sqlite3_bind_int64(insert_stmt, 1, atlas_id);
                sqlite3_bind_int64(insert_stmt, 2, atlas_idx);
                sqlite3_bind_int64(insert_stmt, 3, width);
                sqlite3_bind_int64(insert_stmt, 4, height);
                sqlite3_bind_int64(insert_stmt, 5, channels);
                sqlite3_bind_int64(insert_stmt, 6, image_id);
                db_run_stmt(insert_stmt, 0);
                sqlite3_reset(insert_stmt);
                sqlite3_clear_bindings(insert_stmt);

                sqlite3_bind_int64(slot_stmt, 1, atlas_id);
                sqlite3_bind_int64(slot_stmt, 2, atlas_idx);
                sqlite3_bind_int64(slot_stmt, 3, image_id);
                db_run_stmt(slot_stmt, 0);
                sqlite3_reset(slot_stmt);
                sqlite3_clear_bindings(slot_stmt);

                Image img = {.atlas_id = atlas_id,
                             .atlas_idx = atlas_idx,
                             .width = width,
                             .height = height,
                             .channels = channels};

                dense_update(images, image_id, img);
            }
            sqlite3_finalize(insert_stmt);
            sqlite3_finalize(slot_stmt);

            // NOTE: now, start waiting for last batch job
            os_semaphore_take(batch_sem, U64_MAX);

            if (!row.update && task_count < ATLAS_CAPACITY)
            {
                // NOTE: when new atlas is created and it has empty slots, fill
                // them with NULL values in image_id column
                sqlite3_stmt *slot_stmt = db_prepare("INSERT INTO AtlasSlots (atlas_id, atlas_idx, image_id) VALUES (?, ?, ?) ON CONFLICT(atlas_id, atlas_idx) DO UPDATE SET image_id = excluded.image_id;");

                // NOTE: last index where image was drawn + 1 marks the beginning of
                // empty slots
                U64 empty_start = draw_prm[da_getsize(draw_prm) - 1].draw_idx + 1;
                for (U64 j = empty_start; j < ATLAS_CAPACITY; j++)
                {
                    sqlite3_bind_int64(slot_stmt, 1, row.atlas_id);
                    sqlite3_bind_int64(slot_stmt, 2, j);
                    db_run_stmt(slot_stmt, 0);
                    sqlite3_reset(slot_stmt);
                    sqlite3_clear_bindings(slot_stmt);
                }
                sqlite3_finalize(slot_stmt);
            }
            B32 write_success = ToBool(stbi_write_tga(CStrCast(row.path), ATLAS_SIZE, ATLAS_SIZE, ATLAS_CHANNELS, atlas_data));
            Assert(write_success, "failed to save image (%.*s)", StringSpr(row.path));
            mscbl_log_dbg("Written atlas: %.*s", StringSpr(row.path));

            // NOTE: only writes to DB if everything up till here succeeds
            db_run("COMMIT;");

            // NOTE: load atlas as texture
            dense_update(atlases, row.atlas_id, {.id = row.atlas_id});
            GLA_tex params = {
                .texture = &atlases[row.atlas_id].tex,
                .data = atlas_data,
                .width = ATLAS_SIZE,
                .height = ATLAS_SIZE,
                .channels = ATLAS_CHANNELS};
            gl_push({.kind = GLArgs_tex, .v = params});
        }
    }

    arena_array_free(worker_memory);
    os_semaphore_release(batch_sem);
}
