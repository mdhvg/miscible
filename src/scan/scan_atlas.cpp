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
#include "stb_image_resize2.h"
#include "stb_image_write.h"

#include "base/log.h"
#include "base/array.h"
#include "scan/scan.h"

global_v U8 *atlas_data = NULL;

ThreadFunc(read_image)
{
    Assert(data.kind == TPData_ANY, "wrong datatype");
    ImageRow *row = (ImageRow *)data.any;

    S32 w, h, c;
    S32 resize_height, resize_width;
    U8 *img_data = stbi_load(CStrCast(row->path), &w, &h, &c, 3);
    Assert(img_data, "image data is NULL (%.*s)", StringSpr(row->path));

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

    row->width         = w;
    row->height        = h;
    row->channels      = c;
    row->data          = resize_data;
    row->resize_width  = resize_width;
    row->resize_height = resize_height;
}

struct draw_params
{
    U64 draw_idx;
    ImageRow *row;
};

ThreadFunc(draw_image)
{
    Assert(data.kind == TPData_ANY, "wrong datatype");
    draw_params params = *(draw_params *)data.any;
    U32 smaller_side   = MIN(params.row->resize_width, params.row->resize_height);

    U32 x_off = (params.row->resize_width - smaller_side) / 2;
    U32 y_off = (params.row->resize_height - smaller_side) / 2;

    U8 *src = params.row->data + (y_off * params.row->resize_width + x_off) * 3;
    U8 *dst = atlas_data + (params.draw_idx / THUMB_PER_SIDE * ATLAS_SIZE * 3 * THUMB_SIZE) +
              (params.draw_idx % THUMB_PER_SIDE * 3 * THUMB_SIZE);

    for (U32 i = 0; i < THUMB_SIZE; i++)
    {
        memcpy(dst, src, THUMB_SIZE * 3);
        src += params.row->resize_width * 3;
        dst += ATLAS_SIZE * 3;
    }
}

struct AtlasRow
{
    S64 atlas_id;
    String path;
    B32 update;
};

struct atlas_params
{
    Arena *arena;
    AtlasRow *row;
};

DBStmtCbk(get_atlas)
{
    atlas_params *params = (atlas_params *)data;
    AtlasRow row         = {
        sqlite3_column_int64(stmt, 0),
        string_cpy(params->arena, sqlite3_column_text(stmt, 1)),
        1};
    *params->row = row;
}

struct idx_params
{
    Arena *arena;
    draw_params *array;
};

DBStmtCbk(push_idx)
{
    idx_params *params = (idx_params *)data;
    draw_params idx    = {(U64)sqlite3_column_int64(stmt, 0)};
    da_push(params->arena, params->array, idx);
}

void scan_atlas_bake(Arena *arena, ImageRow *inserted)
{
    U64 inserted_count = da_getsize(inserted);

    Temp tmp              = temp_begin(arena);
    AtlasRow row          = {0};
    draw_params *draw_pos = NULL;

    U64 task_count      = 0;
    Semaphore batch_sem = os_semaphore_alloc(0, S32_MAX);

    for (U64 i = 0; i < inserted_count; i += ATLAS_CAPACITY)
    {
        threadpool_clear_arenas();

        row      = {0};
        draw_pos = NULL;
        da_setcap(tmp.arena, draw_pos, ATLAS_CAPACITY);

        task_count = MIN(i + ATLAS_CAPACITY, inserted_count) - i;
        for (U64 j = i; j < MIN(i + ATLAS_CAPACITY, inserted_count); j++)
        {
            TPData args    = {.kind = TPData_ANY, .any = inserted + j};
            AsyncTask task = {read_image, args, &task_count, batch_sem};
            threadpool_enqueue(task);
        }

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
        sqlite3_bind_int64(st0, 1, inserted_count - i);
        atlas_params p = {tmp.arena, &row};
        if (db_run_stmt(st0, 1, get_atlas, &p))
        {
            sqlite3_stmt *st1 = db_prepare(
                "SELECT atlas_idx "
                "FROM AtlasSlots "
                "WHERE atlas_id = ?;");
            sqlite3_bind_int64(st1, 1, row.atlas_id);
            idx_params _p = {tmp.arena, draw_pos};
            db_run_stmt(st1, 1, push_idx, &_p);

            S32 w, h, c;
            atlas_data = stbi_load(CStrCast(row.path), &w, &h, &c, 3);
        }

        if (!da_getsize(draw_pos))
        {
            char *guid = push_array(tmp.arena, sizeof(Guid) * 2, char);
            bytes_as_hex_lower(os_make_guid().v, 0, sizeof(Guid), guid);

            StringBuilder path = string_empty(tmp.arena);
            string_format(&path, "%.*s/%.*s.tga", StringSpr(mscbl_config.atlas_dir), sizeof(Guid) * 2, guid);

            row = {-1, StringCast(path), 0};

            for (U64 j = 0; j < MIN(ATLAS_CAPACITY, inserted_count - i); j++)
            {
                draw_params pos = {j};
                da_push(tmp.arena, draw_pos, pos);
            }

            atlas_data = push_array(tmp.arena, (ATLAS_SIZE * ATLAS_SIZE * 3), U8);
        }
        os_semaphore_take(batch_sem, U64_MAX);

        task_count = MIN(i + ATLAS_CAPACITY, inserted_count) - i;
        for (U64 j = i; j < MIN(i + ATLAS_CAPACITY, inserted_count); j++)
        {
            draw_pos[j - i].row = inserted + j;
            TPData args         = {.kind = TPData_ANY, .any = draw_pos + j - i};
            threadpool_enqueue({draw_image, args, &task_count, batch_sem});
        }

        // Now, save the atlas and update the database

        db_run("BEGIN TRANSACTION;");

        // Insert atlas if new
        if (!row.update)
        {
            sqlite3_stmt *st2 = db_prepare("INSERT INTO Atlas(atlas_path) VALUES(?) RETURNING id;");
            sqlite3_bind_text(st2, 1, CStrCast(row.path), row.path.size, SQLITE_STATIC);
            db_run_stmt(st2, 1, get_id, &row.atlas_id);
        }

        // Insert images and Create slot entries
        sqlite3_stmt *insert_stmt = db_prepare("UPDATE Images SET atlas_id = ?, atlas_idx = ?, width = ?, height = ?, channels = ? WHERE id = ?;");
        sqlite3_stmt *slot_stmt   = db_prepare("INSERT INTO AtlasSlots (atlas_id, atlas_idx, image_id) VALUES (?, ?, ?) ON CONFLICT(atlas_id, atlas_idx) DO UPDATE SET image_id = excluded.image_id;");
        for (U64 j = i; j < MIN(i + ATLAS_CAPACITY, inserted_count); j++)
        {
            S64 image_id  = inserted[j].id;
            S64 atlas_id  = row.atlas_id;
            U32 atlas_idx = draw_pos[j - i].draw_idx;
            S32 width     = inserted[i].width;
            S32 height    = inserted[i].height;
            S32 channels  = inserted[i].channels;

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

            Image img;
            img.atlas_id  = atlas_id;
            img.atlas_idx = atlas_idx;
            img.width     = width;
            img.height    = height;
            img.channels  = channels;

            dense_update(images, image_id, img);
        }
        sqlite3_finalize(insert_stmt);
        sqlite3_finalize(slot_stmt);

        os_semaphore_take(batch_sem, U64_MAX);

        task_count = MIN(i + ATLAS_CAPACITY, inserted_count) - i;
        if (!row.update && task_count < ATLAS_CAPACITY)
        {
            sqlite3_stmt *slot_stmt = db_prepare("INSERT INTO AtlasSlots (atlas_id, atlas_idx, image_id) VALUES (?, ?, ?) ON CONFLICT(atlas_id, atlas_idx) DO UPDATE SET image_id = excluded.image_id;");
            // Can only happen when a new atlas is created and it's not filled completely
            for (U64 j = draw_pos[da_getsize(draw_pos)].draw_idx; j < ATLAS_CAPACITY; j++)
            {
                sqlite3_bind_int64(slot_stmt, 1, row.atlas_id);
                sqlite3_bind_int64(slot_stmt, 2, j);
                db_run_stmt(slot_stmt, 0);
                sqlite3_reset(slot_stmt);
                sqlite3_clear_bindings(slot_stmt);
            }
            sqlite3_finalize(slot_stmt);
        }

        Assert(stbi_write_tga(CStrCast(row.path), ATLAS_SIZE, ATLAS_SIZE, 3, atlas_data), "failed to save image (%.*s)", row.path.size, row.path.v);
        mscbl_log_dbg("Written atlas: %.*s", row.path.size, CStrCast(row.path));
        db_run("COMMIT;");

        // Load atlas as texture
        dense_update(atlases, row.atlas_id, {row.atlas_id});
        GLA_tex params = {&atlases[row.atlas_id].tex, atlas_data, ATLAS_SIZE, ATLAS_SIZE, 3};
        gl_push({GLArgs_tex, params});

        temp_end(tmp);
    }

    os_semaphore_release(batch_sem);
}
