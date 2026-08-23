// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"

#include "base/log.h"
#include "db/fetch.h"
#include "scan/scan.h"
#include "base/array.h"
#include "gl/gl_core.h"
#include "base/string.h"
#include "app/miscible.h"
#include "base/threadpool.h"

ThreadFunc(draw_image)
{
    Assert(args[0].kind == TPData_Any, "wrong datatype");
    Assert(args[1].kind == TPData_Any, "wrong datatype");
    Assert(args[2].kind == TPData_U32, "wrong datatype");

    ImageRow *image_row = (ImageRow *)args[0].val_any;
    U8 *draw_location = (U8 *)args[1].val_any                                                           // Don't format
                        + (args[2].val_u32 / THUMB_PER_SIDE * ATLAS_SIZE * ATLAS_CHANNELS * THUMB_SIZE) // Don't format
                        + (args[2].val_u32 % THUMB_PER_SIDE * ATLAS_CHANNELS * THUMB_SIZE);

    U8 *image_data = stbi_load(CStrCast(image_row->path), &image_row->width, &image_row->height, &image_row->channels, ATLAS_CHANNELS);
    Assert(image_data, "image data is NULL (%.*s)", StringSpr(image_row->path));

    S32 w = image_row->width, h = image_row->height;

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

    U8 *cropped_image_data = image_data + (crop_y * w + crop_x) * ATLAS_CHANNELS;

    stbir_resize_uint8_linear(cropped_image_data, crop_w, crop_h, w * ATLAS_CHANNELS, draw_location, THUMB_SIZE, THUMB_SIZE, ATLAS_SIZE * ATLAS_CHANNELS, STBIR_RGBA);
    stbi_image_free(image_data);
}

struct DrawAtlas
{
    S64 id;
    U8 *data;
    B32 is_new;
    String path;
    U32 indices[ATLAS_CAPACITY];
};

DBStmtCbk(get_atlas)
{
    DrawAtlas *ret = (DrawAtlas *)data;
    DrawAtlas row = {
        .id = sqlite3_column_int64(stmt, 0),
        .path = string_copy(arena, sqlite3_column_text(stmt, 1)),
    };
    *ret = row;
}

DBStmtCbk(push_idx)
{
    U32 **idx_start_ptr = (U32 **)data;
    **idx_start_ptr = sqlite3_column_int64(stmt, 0);
    *idx_start_ptr = *idx_start_ptr + 1;
}

static const char *find_atlas_query = "SELECT s.atlas_id, a.atlas_path "
                                      "FROM AtlasSlots s JOIN Atlas a "
                                      "ON s.atlas_id = a.id "
                                      "WHERE s.atlas_id IN "
                                      "(SELECT atlas_id "
                                      "FROM AtlasSlots "
                                      "WHERE image_id IS NULL "
                                      "GROUP BY atlas_id "
                                      "HAVING COUNT(*) >= ?);";

static const char *make_atlas_query = "INSERT INTO Atlas(atlas_path) "
                                      "VALUES(?) RETURNING id;";

static const char *make_slots_query = "INSERT INTO AtlasSlots(atlas_id, atlas_idx) "
                                      "WITH RECURSIVE idx(x) AS ("
                                      "SELECT 0 UNION ALL SELECT x+1 FROM idx "
                                      "WHERE x<" Stringify(ATLAS_CAPACITY) "-1) SELECT ?, x FROM idx;";

DrawAtlas scan_get_draw_atlas(Arena *arena, S64 num_image)
{
    DrawAtlas atlas = {.id = 0};

    sqlite3_stmt *stmt = db_prepare(find_atlas_query);
    sqlite3_bind_int64(stmt, 1, num_image);
    if (db_run_stmt(stmt, 1, get_atlas, &atlas, arena))
    {
        U32 *idx_start_ptr = &atlas.indices[0];
        stmt = db_prepare("SELECT atlas_idx "
                          "FROM AtlasSlots "
                          "WHERE atlas_id = ? "
                          "AND image_id IS NULL;");
        sqlite3_bind_int64(stmt, 1, atlas.id);
        db_run_stmt(stmt, 1, push_idx, &idx_start_ptr);

        atlas.is_new = 0;
        S32 w, h, c;
        atlas.data = stbi_load(CStrCast(atlas.path), &w, &h, &c, ATLAS_CHANNELS);
    }
    else
    {
        StringBuilder new_atlas_path = string_init(arena, mscbl_config.atlas_dir);
        U64 new_atlas_filename_begin = new_atlas_path.size + 1;
        path_join(&new_atlas_path, sv("00000000000000000000000000000000.tga"));
        bytes_as_hex_lower(sizeof(Guid), os_make_guid().v, CStrCast(string_from(StringCast(new_atlas_path), new_atlas_filename_begin)));
        atlas.path = StringCast(new_atlas_path);

        stmt = db_prepare(make_atlas_query);
        sqlite3_bind_text(stmt, 1, CStrCast(new_atlas_path), new_atlas_path.size, SQLITE_STATIC);
        db_run_stmt(stmt, 1, get_id, &atlas.id);

        stmt = db_prepare(make_slots_query);
        sqlite3_bind_int64(stmt, 1, atlas.id);
        db_run_stmt(stmt, 1);

        atlas.is_new = 1;
        atlas.data = push_array(arena, ATLAS_SIZE * ATLAS_SIZE * ATLAS_CHANNELS, U8);

        for (U32 i = 0; i < ATLAS_CAPACITY; i++)
            atlas.indices[i] = i;
    }

    return atlas;
}

static const char *image_update_query = "UPDATE Images "
                                        "SET atlas_id = ?, atlas_idx = ?, "
                                        "width = ?, height = ?, channels = ? "
                                        "WHERE id = ?;";

static const char *slot_update_query = "UPDATE AtlasSlots "
                                       "SET image_id = ? "
                                       "WHERE atlas_id = ? AND atlas_idx = ?;";

void scan_atlas_bake(Arena *arena, ImageRow *inserted)
{
    S64 total_image_count = arr_getsize(inserted);
    Semaphore batch_sem = os_semaphore_init(0, S32_MAX);

    for (U64 base = 0; base < total_image_count; base += ATLAS_CAPACITY)
    {
        ArenaScoped(arena)
        {
            // NOTE: get number of images to be drawn (0, 100]
            S64 task_count = MIN(base + ATLAS_CAPACITY, total_image_count) - base;

            // NOTE: find the existing atlas which can accomodate the images of this batch or make new
            DrawAtlas atlas = scan_get_draw_atlas(arena, task_count);

            S64 batch_size = task_count;
            for (S64 off = 0; off < task_count; off++)
            {
                // NOTE: draw jobs are taken from entire inserted array
                // so job index = base + off (offset of current job)
                AsyncTask task = {
                    .func = draw_image,
                    .args = {
                        {.kind = TPData_Any, .val_any = &inserted[base + off]},
                        {.kind = TPData_Any, .val_any = atlas.data},
                        {.kind = TPData_U32, .val_u32 = atlas.indices[off]},
                    },
                    .batch_size = &batch_size,
                    .batch_complete = batch_sem};
                threadpool_enqueue(TaskPriority_High, task);
            }

            // NOTE: wait until batch of draw_image commands finishes
            os_semaphore_pop(batch_sem, U64_MAX);

            // NOTE: save the atlas and update the database as 1 transaction
            db_run("BEGIN TRANSACTION;");

            //NOTE: insert images and Create slot entries
            sqlite3_stmt *insert_stmt = db_prepare(image_update_query);
            sqlite3_stmt *slot_stmt = db_prepare(slot_update_query);
            for (S64 off = 0; off < task_count; off++)
            {
                S64 image_id = inserted[off + base].id;
                S64 atlas_id = atlas.id;
                U32 atlas_idx = atlas.indices[off];
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

                sqlite3_bind_int64(slot_stmt, 1, image_id);
                sqlite3_bind_int64(slot_stmt, 2, atlas_id);
                sqlite3_bind_int64(slot_stmt, 3, atlas_idx);
                db_run_stmt(slot_stmt, 0);
                sqlite3_reset(slot_stmt);
                sqlite3_clear_bindings(slot_stmt);
            }
            sqlite3_finalize(insert_stmt);
            sqlite3_finalize(slot_stmt);

            B32 write_success = ToBool(stbi_write_tga(CStrCast(atlas.path), ATLAS_SIZE, ATLAS_SIZE, ATLAS_CHANNELS, atlas.data));
            Assert(write_success, "failed to save image (%.*s)", StringSpr(atlas.path));
            mscbl_log_info("Written atlas: %.*s", StringSpr(atlas.path));

            // NOTE: only writes to DB if everything up till here succeeds
            db_run("COMMIT;");

            // NOTE: load atlas as texture
            AtlasMap map = {.id = atlas.id};
            gl_tex_data(&map.tex, atlas.data, ATLAS_SIZE, ATLAS_SIZE, ATLAS_CHANNELS);
            dense_update(fetch_atlases, atlas.id, map);
            if (!atlas.is_new)
                stbi_image_free(atlas.data);
        }
    }
}
