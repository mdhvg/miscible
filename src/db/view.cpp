// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "db/view.h"
#include "app/miscible.h"
#include "base/arena.h"
#include "base/base_core.h"
#include "base/log.h"
#include "config.h"
#include "db/db_helpers.h"
#include "base/array.h"
#include "base/string.h"
#include "base/threadpool.h"
#include "db/fetch.h"
#include "inference/inference.h"
#include "inference/model.h"
#include "ui/ui_core.h"

#define VIEW_FETCH_WINDOW_SIZE 1000

void view_serialize_filters(Arena *arena, UIFilter *filters, StringArr *queries)
{
    for (UIFilter *f0 = filters; f0 != NULL; f0 = f0->next)
    {
        if (!f0->active)
            continue;

        switch (f0->type)
        {
        case FilterType_SizeBetween:
            if (f0->val_byte.from_enable)
                da_push(arena, *queries, sv(" AND size >= ?"));
            if (f0->val_byte.to_enable)
                da_push(arena, *queries, sv(" AND size < ?"));
            break;
        case FilterType_Path:
            if (!f0->val_str.val.size)
                continue;
            da_push(arena, *queries, sv(" AND Images.id"));
            if (f0->val_str.exclude)
                da_push(arena, *queries, sv(" NOT"));
            da_push(arena, *queries, sv(" IN (SELECT rowid FROM Image_FTS WHERE Image_FTS MATCH ?)"));
            break;
        case FilterType_DateAddedBetween:
            if (f0->val_time.from_enable)
                da_push(arena, *queries, sv(" AND atime >= ?"));
            if (f0->val_time.to_enable)
                da_push(arena, *queries, sv(" AND atime < ?"));
            break;
        case FilterType_DateCreatedBetween:
            if (f0->val_time.from_enable)
                da_push(arena, *queries, sv(" AND ctime >= ?"));
            if (f0->val_time.to_enable)
                da_push(arena, *queries, sv(" AND ctime < ?"));
            break;
        case FilterType_DateModifiedBetween:
            if (f0->val_time.from_enable)
                da_push(arena, *queries, sv(" AND mtime >= ?"));
            if (f0->val_time.to_enable)
                da_push(arena, *queries, sv(" AND mtime < ?"));
            break;
        default: break;
        }
    }
}

void view_serialize_sort(Arena *arena, UIViewQuery *request, StringArr *queries)
{
    switch (request->sort_basis)
    {
    case SortType_Directory:
        da_push(arena, *queries, sv(" Dirs.path"));
        break;
    case SortType_Filename:
        da_push(arena, *queries, sv(" UPPER(SUBSTR(Images.filename, 1, 1))"));
        break;
    case SortType_Size:
        da_push(arena, *queries, sv(" (Images.size / ?) * ?"));
        break;
    case SortType_DateAdded:
        da_push(arena, *queries, sv(" (Images.atime / ?) * ?"));
        break;
    case SortType_DateCreated:
        da_push(arena, *queries, sv(" (Images.ctime / ?) * ?"));
        break;
    case SortType_DateModified:
        da_push(arena, *queries, sv(" (Images.mtime / ?) * ?"));
        break;
    default: break;
    }
}

String view_build_query_default(Arena *arena, B32 count_only)
{
    StringArr queries = NULL;
    UIViewQuery *request = &ui_state.view_query;

    da_push(arena, queries, sv("SELECT"));
    view_serialize_sort(arena, request, &queries);
    da_push(arena, queries, sv(" AS header,"));

    if (count_only)
        da_push(arena, queries, sv(" COUNT(*) AS item_count"));
    else
        da_push(arena, queries, sv(" Images.*"));

    da_push(arena, queries, sv(" FROM Images"));

    if (request->sort_basis == SortType_Directory)
        da_push(arena, queries, sv(" JOIN Dirs ON Images.parent_dir = Dirs.id"));

    da_push(arena, queries, sv(" WHERE 1=1"));

    view_serialize_filters(arena, request->filters, &queries);
    if (request->selected_dir)
        da_push(arena, queries, sv(" AND Images.path LIKE (SELECT Dirs.path FROM Dirs WHERE Dirs.id = ?) || '" OSSlash "%'"));

    if (count_only)
    {
        da_push(arena, queries, sv(" GROUP BY header HAVING item_count > 0 ORDER BY header"));
        da_push(arena, queries, request->descending ? sv(" DESC;") : sv(" ASC;"));
    }
    else
    {
        da_push(arena, queries, sv(" ORDER BY header"));
        da_push(arena, queries, request->descending ? sv(" DESC") : sv(" ASC"));
        da_push(arena, queries, sv(" , Images.id ASC LIMIT ? OFFSET ?;"));
    }

    StringBuilder query = string_empty(arena, KB(4));
    for (S64 i = 0; i < arr_getsize(queries); i++)
        string_push(&query, queries[i]);

    return StringCast(query);
}

String view_build_query_fts(Arena *arena, B32 count_only)
{
    String *queries = NULL;
    UIViewQuery *request = &ui_state.view_query;

    da_push(arena, queries, sv("SELECT"));
    view_serialize_sort(arena, request, &queries);
    da_push(arena, queries, sv(" AS header,"));

    // Choosing item count or metadata
    if (count_only)
        da_push(arena, queries, sv(" COUNT(*) AS item_count"));
    else
        da_push(arena, queries, sv(" Images.*"));

    da_push(arena, queries, sv(" FROM Images"));

    if (request->sort_basis == SortType_Directory)
        da_push(arena, queries, sv(" JOIN Dirs ON Images.parent_dir = Dirs.id"));

    da_push(arena, queries, sv(" WHERE Images.id IN (SELECT rowid FROM Image_FTS WHERE Image_FTS MATCH ?)"));

    // Apply filters
    view_serialize_filters(arena, ui_state.view_query.filters, &queries);
    if (request->selected_dir)
        da_push(arena, queries, sv(" AND Images.path LIKE (SELECT Dirs.path FROM Dirs WHERE Dirs.id = ?) || '" OSSlash "%'"));

    // Set order and limits
    if (count_only)
    {
        da_push(arena, queries, sv(" GROUP BY header HAVING item_count > 0 ORDER BY header"));
        da_push(arena, queries, request->descending ? sv(" DESC;") : sv(" ASC;"));
    }
    else
    {
        da_push(arena, queries, sv(" ORDER BY header"));
        da_push(arena, queries, request->descending ? sv(" DESC") : sv(" ASC"));
        da_push(arena, queries, sv(" , Images.id ASC LIMIT ? OFFSET ?;"));
    }

    StringBuilder query = string_empty(arena, KB(4));
    for (S64 i = 0; i < arr_getsize(queries); i++)
        string_push(&query, queries[i]);

    return StringCast(query);
}

String view_build_query_embedding(Arena *arena, B32 count_only)
{
    String *queries = NULL;
    UIViewQuery *request = &ui_state.view_query;

    da_push(arena, queries, sv("SELECT CAST((1.0 - distance_cosine_f32(Images.embedding, ?)) * 10 AS INT) * 10 AS header,"));

    // Choosing item count or metadata
    if (count_only)
        da_push(arena, queries, sv(" COUNT(*) AS item_count"));
    else
        da_push(arena, queries, sv(" Images.*"));
    da_push(arena, queries, sv(" , distance_cosine_f32(Images.embedding, ?) AS distance FROM Images"));

    if (request->sort_basis == SortType_Directory)
        da_push(arena, queries, sv(" JOIN Dirs ON Images.parent_dir = Dirs.id"));
    da_push(arena, queries, sv(" WHERE Images.embedding IS NOT NULL"));

    // Apply filters
    view_serialize_filters(arena, ui_state.view_query.filters, &queries);
    if (request->selected_dir)
        da_push(arena, queries, sv(" AND Images.path LIKE (SELECT Dirs.path FROM Dirs WHERE Dirs.id = ?) || '" OSSlash "%'"));

    // Set order and limits
    if (count_only)
        da_push(arena, queries, sv(" GROUP BY header HAVING item_count > 0 ORDER BY header DESC;"));
    else
        da_push(arena, queries, sv(" ORDER BY distance DESC LIMIT ? OFFSET ?;"));

    StringBuilder query = string_empty(arena, KB(4));
    for (S64 i = 0; i < arr_getsize(queries); i++)
        string_push(&query, queries[i]);

    return StringCast(query);
}

String view_build_query(Arena *arena, B32 count_only)
{
    switch (ui_state.view_query.query_type)
    {
    case ViewQuery_Default: return view_build_query_default(arena, count_only);
    case ViewQuery_FTS: return view_build_query_fts(arena, count_only);
    case ViewQuery_Embedding: return view_build_query_embedding(arena, count_only);
    }
}

DBStmtCbk(push_image_map)
{
    arena = ui_result.back_map.arena;
    S64 *global_offset = (S64 *)data;
    S64 item_count = sqlite3_column_int64(stmt, 1);

    UIMap map = {.item_count = item_count,
                 .global_group_offset = *global_offset};
    *global_offset = *global_offset + item_count;

    if (ui_state.view_query.query_type == ViewQuery_Embedding)
    {
        map.s32_header = sqlite3_column_int(stmt, 0);
    }
    else
    {
        switch (ui_state.view_query.sort_basis)
        {
        case SortType_Directory:
        case SortType_Filename:
            map.str_header = string_copy(arena, sqlite3_column_text(stmt, 0));
            break;
        case SortType_Size:
            map.size_header = size_to_bytesize(sqlite3_column_int64(stmt, 0));
            break;
        case SortType_DateAdded:
        case SortType_DateCreated:
        case SortType_DateModified:
            map.time_header = timestamp_to_time(sqlite3_column_int64(stmt, 0));
            break;
        default: break;
        }
    }

    da_push(arena, ui_result.back_map.map, map);
}

DBStmtCbk(push_image_list)
{
    arena = ui_result.back_list.arena;

    S32 cursor = 1;
    S64 image_id = sqlite3_column_int64(stmt, cursor++);

    String image_path = string_copy(arena, sqlite3_column_text(stmt, cursor++));
    String image_filename = string_copy(arena, sqlite3_column_text(stmt, cursor++));

    S64 image_atlas_id = sqlite3_column_int64(stmt, cursor++);
    U32 image_atlas_idx = (U32)sqlite3_column_int(stmt, 5);

    ByteSize image_size = size_to_bytesize(sqlite3_column_int64(stmt, cursor++));

    Time image_ctime = timestamp_to_time(sqlite3_column_int64(stmt, cursor++));
    Time image_mtime = timestamp_to_time(sqlite3_column_int64(stmt, cursor++));
    Time image_atime = timestamp_to_time(sqlite3_column_int64(stmt, cursor++));

    S32 image_width = sqlite3_column_int(stmt, cursor++);
    S32 image_height = sqlite3_column_int(stmt, cursor++);
    S32 image_channels = sqlite3_column_int(stmt, cursor++);

    ImageMetadata metadata = {
        .id = image_id,
        .path = image_path,
        .filename = image_filename,

        .atlas_id = image_atlas_id,
        .atlas_idx = image_atlas_idx,

        .size = image_size,

        .ctime = image_ctime,
        .mtime = image_mtime,
        .atime = image_atime,

        .width = image_width,
        .height = image_height,
        .channels = image_channels,
    };

    da_push(arena, ui_result.back_list.images, metadata);

    // if (ui_state.view_query.query_type == ViewQuery_Embedding)
    // {
    //     S32 last_col_idx = sqlite3_column_count(stmt) - 1;
    //     mscbl_log_info("distance: %.2f", sqlite3_column_double(stmt, last_col_idx));
    // }
}

S32 view_bind_filters(sqlite3_stmt *stmt, UIViewQuery *request, S32 cursor)
{
    for (UIFilter *f0 = request->filters; f0 != NULL; f0 = f0->next)
    {
        if (!f0->active)
            continue;

        switch (f0->type)
        {
        case FilterType_SizeBetween:
            if (f0->val_byte.from_enable)
            {
                U64 bytes = bytesize_to_size(f0->val_byte.from);
                sqlite3_bind_int64(stmt, cursor++, bytes);
            }
            if (f0->val_byte.from_enable)
            {
                U64 bytes = bytesize_to_size(f0->val_byte.to);
                sqlite3_bind_int64(stmt, cursor++, bytes);
            }
            break;
        case FilterType_Path: {
            if (!f0->val_str.val.size)
                continue;
            String value = StringCast(f0->val_str.val);
            sqlite3_bind_text(stmt, cursor++, CStrCast(value), value.size, SQLITE_STATIC);
        }
        break;
        case FilterType_DateAddedBetween:
        case FilterType_DateCreatedBetween:
        case FilterType_DateModifiedBetween:
            if (f0->val_time.from_enable)
            {
                U64 timestamp = time_to_timestamp(f0->val_time.from);
                sqlite3_bind_int64(stmt, cursor++, timestamp);
            }
            if (f0->val_time.to_enable)
            {
                U64 timestamp = time_to_timestamp(f0->val_time.to);
                sqlite3_bind_int64(stmt, cursor++, timestamp);
            }
            break;
        default: break;
        }
    }
    return cursor;
}

S32 view_bind_sort(sqlite3_stmt *stmt, UIViewQuery *request, S32 cursor)
{
    switch (request->sort_basis)
    {
    case SortType_Size: {
        U64 bytes = request->sub_type;
        sqlite3_bind_int64(stmt, cursor++, bytes);
        sqlite3_bind_int64(stmt, cursor++, bytes);
    }
    break;
    case SortType_DateAdded:
    case SortType_DateCreated:
    case SortType_DateModified: {
        U64 time = request->sub_type;
        sqlite3_bind_int64(stmt, cursor++, time);
        sqlite3_bind_int64(stmt, cursor++, time);
    }
    break;
    default: break;
    }

    return cursor;
}

void view_run_query_default(Arena *arena, sqlite3_stmt *stmt, B32 count_only)
{
    UIViewQuery *request = &ui_state.view_query;

    S32 cursor = 1;
    cursor = view_bind_sort(stmt, request, cursor);
    cursor = view_bind_filters(stmt, request, cursor);
    if (request->selected_dir)
        sqlite3_bind_int(stmt, cursor++, request->selected_dir);

    if (!count_only)
    {
        sqlite3_bind_int64(stmt, cursor++, ui_state.view_query.visible_end - ui_state.view_query.visible_start);
        sqlite3_bind_int64(stmt, cursor++, ui_state.view_query.visible_start);
    }

    if (count_only)
    {
        S64 global_offset = 0;
        ui_result.back_map.ticket = ui_state.view_query.ticket;
        ui_result.back_map.query_type = ViewQuery_Default;
        db_run_stmt(stmt, 1, push_image_map, &global_offset);
        UIMap guard = {.global_group_offset = global_offset};
        da_push(ui_result.back_map.arena, ui_result.back_map.map, guard);

        UIMapResult temp = ui_result.back_map;
        ui_result.back_map = ui_result.main_map;
        ui_result.main_map = temp;
    }
    else
    {
        db_run_stmt(stmt, 1, push_image_list);
        UIMetadataResult temp = ui_result.back_list;
        ui_result.back_list = ui_result.main_list;
        ui_result.main_list = temp;
    }
}

void view_run_query_fts(Arena *arena, sqlite3_stmt *stmt, B32 count_only)
{
    UIViewQuery *request = &ui_state.view_query;

    S32 cursor = 1;
    cursor = view_bind_sort(stmt, request, cursor);
    sqlite3_bind_text(stmt, cursor++, CStrCast(request->search_query), request->search_query.size, SQLITE_STATIC);
    cursor = view_bind_filters(stmt, request, cursor);
    if (request->selected_dir)
        sqlite3_bind_int(stmt, cursor++, request->selected_dir);

    if (!count_only)
    {
        sqlite3_bind_int64(stmt, cursor++, ui_state.view_query.visible_end - ui_state.view_query.visible_start);
        sqlite3_bind_int64(stmt, cursor++, ui_state.view_query.visible_start);
    }

    if (count_only)
    {
        S64 global_offset = 0;
        ui_result.back_map.ticket = ui_state.view_query.ticket;
        ui_result.back_map.query_type = ViewQuery_FTS;
        db_run_stmt(stmt, 1, push_image_map, &global_offset);
        UIMap guard = {.global_group_offset = global_offset};
        da_push(ui_result.back_map.arena, ui_result.back_map.map, guard);

        UIMapResult temp = ui_result.back_map;
        ui_result.back_map = ui_result.main_map;
        ui_result.main_map = temp;
    }
    else
    {
        mscbl_log_info("image results: %zu", db_run_stmt(stmt, 1, push_image_list));
        UIMetadataResult temp = ui_result.back_list;
        ui_result.back_list = ui_result.main_list;
        ui_result.main_list = temp;
    }
}

void view_run_query_embedding(Arena *arena, sqlite3_stmt *stmt, B32 count_only)
{
    UIViewQuery *request = &ui_state.view_query;

    S32 cursor = 1;
    Embedding text_embedding = {0};
    text_embedding = inference_text_embedding(arena, StringCast(request->search_query));
    if (text_embedding.dimension == 0)
        return;

    sqlite3_bind_blob(stmt, cursor++, text_embedding.vector, text_embedding.dimension * text_embedding.batch_size * sizeof(F32), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, cursor++, text_embedding.vector, text_embedding.dimension * text_embedding.batch_size * sizeof(F32), SQLITE_STATIC);
    cursor = view_bind_filters(stmt, request, cursor);
    if (request->selected_dir)
        sqlite3_bind_int(stmt, cursor++, request->selected_dir);
    if (!count_only)
    {
        sqlite3_bind_int64(stmt, cursor++, ui_state.view_query.visible_end - ui_state.view_query.visible_start);
        sqlite3_bind_int64(stmt, cursor++, ui_state.view_query.visible_start);
    }

    if (count_only)
    {
        S64 global_offset = 0;
        ui_result.back_map.ticket = ui_state.view_query.ticket;
        ui_result.back_map.query_type = ViewQuery_Embedding;
        db_run_stmt(stmt, 1, push_image_map, &global_offset);
        UIMap guard = {.global_group_offset = global_offset};
        da_push(ui_result.back_map.arena, ui_result.back_map.map, guard);

        UIMapResult temp = ui_result.back_map;
        ui_result.back_map = ui_result.main_map;
        ui_result.main_map = temp;
    }
    else
    {
        mscbl_log_info("image results: %zu", db_run_stmt(stmt, 1, push_image_list));
        UIMetadataResult temp = ui_result.back_list;
        ui_result.back_list = ui_result.main_list;
        ui_result.main_list = temp;
    }
}

ThreadFunc(view_execute)
{
    Assert(args[0].kind == TPData_Any, "wrong datatype");
    Assert(args[1].kind == TPData_B8, "wrong datatype");

    sqlite3_stmt *stmt = (sqlite3_stmt *)args[0].val_any;

    switch (ui_state.view_query.query_type)
    {
    case ViewQuery_Default: return view_run_query_default(arena, stmt, args[1].val_b8);
    case ViewQuery_FTS: return view_run_query_fts(arena, stmt, args[1].val_b8);
    case ViewQuery_Embedding: return view_run_query_embedding(arena, stmt, args[1].val_b8);
    }
}

void view_fetch_map()
{
    Arena *arena = ui_result.back_map.arena;
    arena_clear(arena);
    ui_result.back_map = {0};
    ui_result.back_map.arena = arena;

    if (ui_state.view_query.search_query.size)
    {
        switch (ui_state.view_query.search_type)
        {
        case SearchType_FTS: ui_state.view_query.query_type = ViewQuery_FTS; break;
        case SearchType_Embedding: ui_state.view_query.query_type = ViewQuery_Embedding; break;
        default: ui_state.view_query.query_type = ViewQuery_Default; break;
        }
    }
    else
    {
        ui_state.view_query.query_type = ViewQuery_Default;
    }

    ui_state.view_query.ticket++;

    sqlite3_stmt *stmt = db_prepare(CStrCast(view_build_query(arena, 1)));
    if (!stmt) return;
    arena_clear(arena);

    AsyncTask task = {
        .func = view_execute,
        .args = {
            {.kind = TPData_Any, .val_any = stmt},
            {.kind = TPData_B8, .val_b8 = 1},
        }};
    threadpool_enqueue(TaskPriority_Realtime, task);
}

void view_fetch_images(S64 start, S64 end)
{
    S64 fetch_start = (start / VIEW_FETCH_WINDOW_SIZE) * VIEW_FETCH_WINDOW_SIZE;
    S64 fetch_end = ToCeilInt(end, VIEW_FETCH_WINDOW_SIZE) * VIEW_FETCH_WINDOW_SIZE;

    if (ui_state.view_query.ticket == ui_result.main_map.ticket && ui_state.view_query.visible_start <= fetch_start && ui_state.view_query.visible_end >= fetch_end)
    {
        return;
    }

    ui_state.view_query.visible_start = fetch_start;
    ui_state.view_query.visible_end = fetch_end;

    Arena *arena = ui_result.back_list.arena;
    arena_clear(arena);
    ui_result.back_list = {0};
    ui_result.back_list.arena = arena;

    if (ui_state.view_query.search_query.size)
    {
        switch (ui_state.view_query.search_type)
        {
        case SearchType_FTS: ui_state.view_query.query_type = ViewQuery_FTS; break;
        case SearchType_Embedding: ui_state.view_query.query_type = ViewQuery_Embedding; break;
        default: ui_state.view_query.query_type = ViewQuery_Default; break;
        }
    }
    else
    {
        ui_state.view_query.query_type = ViewQuery_Default;
    }

    sqlite3_stmt *stmt = db_prepare(CStrCast(view_build_query(arena, 0)));
    if (!stmt) return;
    arena_clear(arena);

    AsyncTask task = {
        .func = view_execute,
        .args = {
            {.kind = TPData_Any, .val_any = stmt},
            {.kind = TPData_B8, .val_b8 = 0},
        }};
    threadpool_enqueue(TaskPriority_Realtime, task);
}
