// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "db/view.h"
#include "base/arena.h"
#include "base/base_core.h"
#include "base/log.h"
#include "config.h"
#include "db/db_helpers.h"
#include "base/array.h"
#include "base/string.h"
#include "base/threadpool.h"
#include "db/fetch.h"
#include "inference/model.h"
#include "ui/ui_core.h"

ViewManager view = {0};

void view_init()
{
    arena_alloc(MB(1), view.main.arena);
    arena_alloc(MB(1), view.back.arena);
    arena_alloc(MB(1), view.state.arena);
    view.busy = 0;

    // view_clear_state();
}

// void view_clear_state()
// {
//     if (ins_atomic_u32_eval_cond_assign(&view.busy, 1, 0) == 1)
//         return;
//
//     view.state.search_query = {0};
//     view.state.embedding = {0};
//     view.state.sort_basis = mscbl_config.view_settings.sort_basis;
//     view.state.descending = mscbl_config.view_settings.descending;
//     view.state.filters = 0;
//
//     view.state.ticket++;
//
//     arena_clear(view.state.arena);
//
//     ins_atomic_u32_eval_assign(&view.busy, 0);
// }

// void view_set_state(UIViewQuery ui_query)
// {
//     if (ins_atomic_u32_eval_cond_assign(&view.busy, 1, 0) == 1)
//         return;
//
//     view.state.search_query = string_copy(view.state.arena, StringCast(ui_query.search_query));
//     view.state.sort_basis = (SortType)ui_query.sort_basis;
//     view.state.descending = ui_query.descending;
//
//     for (UIFilter *f0 = ui_query.filters; f0 != NULL; f0 = f0->next)
//     {
//         if (!f0->active)
//             continue;
//
//         ViewFilter f1 = {.type = f0->type, .exclude = f0->exclude};
//         switch (f0->type)
//         {
//         case FilterType_SizeBetween:
//             f1.val_bytes = f0->val_bytes;
//             break;
//
//         case FilterType_Path:
//             f1.val_str = string_copy(view.state.arena, StringCast(f0->val_str));
//             break;
//
//         case FilterType_DateCreatedBetween:
//         case FilterType_DateModifiedBetween:
//             f1.val_date = f0->val_date;
//             break;
//
//         case FilterType_EmbeddingDistanceBetween:
//             f1.val_float = f0->val_float;
//             break;
//         default: break;
//         }
//         da_push(view.state.arena, view.state.filters, f1);
//     }
//
//     view.state.ticket++;
//
//     ins_atomic_u32_eval_assign(&view.busy, 0);
// }

String *view_serialize_filters(Arena *arena, UIFilter *filters)
{
    StringArr serialize_array = NULL;

    for (UIFilter *f0 = filters; f0 != NULL; f0 = f0->next)
    {
        if (!f0->active)
            continue;

        switch (f0->type)
        {
        case FilterType_SizeBetween:
            if (f0->val_byte.from_enable)
                da_push(arena, serialize_array, sv(" AND size >= ?"));
            if (f0->val_byte.to_enable)
                da_push(arena, serialize_array, sv(" AND size < ?"));
            break;
        case FilterType_Path:
            da_push(arena, serialize_array, sv(" AND Images.id"));
            if (f0->val_str.exclude)
                da_push(arena, serialize_array, sv(" NOT"));
            da_push(arena, serialize_array, sv(" IN (SELECT rowid FROM Image_FTS WHERE Image_FTS MATCH ?)"));
            break;
        case FilterType_DateAddedBetween:
            if (f0->val_time.from_enable)
                da_push(arena, serialize_array, sv(" AND atime >= ?"));
            if (f0->val_time.to_enable)
                da_push(arena, serialize_array, sv(" AND atime < ?"));
            break;
        case FilterType_DateCreatedBetween:
            if (f0->val_time.from_enable)
                da_push(arena, serialize_array, sv(" AND ctime >= ?"));
            if (f0->val_time.to_enable)
                da_push(arena, serialize_array, sv(" AND ctime < ?"));
            break;
        case FilterType_DateModifiedBetween:
            if (f0->val_time.from_enable)
                da_push(arena, serialize_array, sv(" AND mtime >= ?"));
            if (f0->val_time.to_enable)
                da_push(arena, serialize_array, sv(" AND mtime < ?"));
            break;
        case FilterType_EmbeddingDistanceBetween:
            if (ui_state.view_query.query_type != QueryType_Embedding)
                continue;
            if (f0->val_float.from_enable)
                da_push(arena, serialize_array, sv(" AND distance >= ?"));
            if (f0->val_float.to_enable)
                da_push(arena, serialize_array, sv(" AND distance < ?"));
            break;
        default:
            break;
        }
    }

    return serialize_array;
}

String view_build_default_query(UIViewQuery request, Arena *arena, String *filters, B32 count_only)
{
    String *queries = NULL;

    da_push(arena, queries, sv("SELECT"));
    if (count_only)
    {
        switch (request.sort_basis)
        {
        case SortType_Directory:
            da_push(arena, queries, sv(" Dirs.path"));
            break;
        case SortType_Filename:
            da_push(arena, queries, sv(" UPPER(SUBSTR(filename, 1, 1))"));
            break;
        case SortType_Size:
            da_push(arena, queries, sv(" (size / ?) * ?"));
            break;
        case SortType_DateAdded:
            da_push(arena, queries, sv(" (atime / ?) * ?"));
            break;
        case SortType_DateCreated:
            da_push(arena, queries, sv(" (ctime / ?) * ?"));
            break;
        case SortType_DateModified:
            da_push(arena, queries, sv(" (mtime / ?) * ?"));
            break;
        default:
            break;
        }
        da_push(arena, queries, sv(" AS header, COUNT(*) AS item_count FROM Images"));
    }

    if (request.sort_basis == SortType_Directory)
    {
        da_push(arena, queries, sv(" JOIN Dirs ON Images.parent_dir = Dirs.id"));
    }

    da_push(arena, queries, sv(" WHERE 1=1"));

    for (U32 i = 0; i < da_getsize(filters); i++)
    {
        da_push(arena, queries, filters[i]);
    }

    if (request.selected_dir)
    {
        da_push(arena, queries, sv(" AND Images.path LIKE (SELECT Dirs.path FROM Dirs WHERE Dirs.id = ?) || '" OSSlash "%'"));
    }

    da_push(arena, queries, sv(" GROUP BY header HAVING item_count > 0 ORDER BY header"));
    da_push(arena, queries, request.descending ? sv(" DESC;") : sv(" ASC;"));

    StringBuilder query = string_empty(arena, KB(4));
    for (S64 i = 0; i < da_getsize(queries); i++)
        string_push(&query, queries[i]);

    return StringCast(query);
}

// String view_build_fts_query(UIViewQuery request, Arena *arena, String *filters, B32 count_only)
// {
//     String *queries = NULL;
//
//     if (count_only)
//         da_push(arena, queries, sv("SELECT COUNT(*)"));
//     else
//         da_push(arena, queries, sv("SELECT id, atlas_id, atlas_idx, path, filename, size, ctime, mtime"));
//     da_push(arena, queries, sv(" FROM Images WHERE id IN (SELECT rowid FROM Image_FTS WHERE Image_FTS MATCH ?)"));
//
//     for (U32 i = 0; i < da_getsize(filters); i++)
//         da_push(arena, queries, filters[i]);
//
//     if (count_only)
//     {
//         da_push(arena, queries, sv(" ;"));
//     }
//     else
//     {
//         da_push(arena, queries, sv(" ORDER BY"));
//
//         switch (request.sort_basis)
//         {
//         case SortType_Path:
//             da_push(arena, queries, sv(" path"));
//             break;
//         case SortType_Filename:
//             da_push(arena, queries, sv(" filename"));
//             break;
//         case SortType_Size:
//             da_push(arena, queries, sv(" size"));
//             break;
//         case SortType_DateCreated:
//             da_push(arena, queries, sv(" ctime"));
//             break;
//         case SortType_DateModified:
//             da_push(arena, queries, sv(" mtime"));
//             break;
//         default:
//             Assert(0, "invalid sort order");
//             break;
//         }
//
//         da_push(arena, queries, request.descending ? sv(" DESC") : sv(" ASC"));
//         da_push(arena, queries, sv(" LIMIT ? OFFSET ?;"));
//     }
//
//     StringBuilder query = string_empty(arena, KB(4));
//     for (S64 i = 0; i < da_getsize(queries); i++)
//         string_push(&query, queries[i]);
//
//     return StringCast(query);
// }

// String view_build_embedding_query(UIViewQuery request, Arena *arena, String *filters, B32 count_only)
// {
//     String *queries = NULL;
//
//     if (count_only)
//         da_push(arena, queries, sv("SELECT COUNT(*) FROM (SELECT distance_cosine_f32(embedding, ?) AS distance"));
//     else
//         da_push(arena, queries, sv("SELECT id, atlas_id, atlas_idx, path, filename, size, ctime, mtime, distance_cosine_f32(embedding, ?) AS distance"));
//     da_push(arena, queries, sv(" FROM Images WHERE embedding IS NOT NULL"));
//
//     for (S64 i = 0; i < da_getsize(filters); i++)
//         da_push(arena, queries, filters[i]);
//
//     if (count_only)
//         da_push(arena, queries, sv(" );"));
//     else
//     {
//         da_push(arena, queries, sv(" ORDER BY distance"));
//         da_push(arena, queries, request.descending ? sv(" DESC") : sv(" ASC"));
//         da_push(arena, queries, sv(" LIMIT ? OFFSET ?;"));
//     }
//
//     StringBuilder query = string_empty(arena, KB(4));
//     for (S64 i = 0; i < da_getsize(queries); i++)
//         string_push(&query, queries[i]);
//
//     return StringCast(query);
// }

// String view_build_query(QueryType query_type, B32 count_only = 0)
// {
//     ViewQuery request = view.state;
//     Arena *arena = view.state.arena;
//     String *filters = view_serialize_filters(arena, request.filters);
//
//     switch (query_type)
//     {
//     case QueryType_Default:
//         return view_build_default_query(request, arena, filters, count_only);
//     case QueryType_Embedding:
//         return view_build_embedding_query(request, arena, filters, count_only);
//     case QueryType_FTS:
//         return view_build_fts_query(request, arena, filters, count_only);
//     default:
//         Assert(0, "wrong enum type %d", query_type);
//         return {0};
//     }
// }

// DBStmtCbk(view_set_count)
// {
//     QueryType query_type = *(QueryType *)data;
//
//     S64 count = sqlite3_column_int64(stmt, 0);
//     view.back.count[query_type] = count;
// }

// DBStmtCbk(view_push_data)
// {
//     // tagged_query query = *(tagged_query *)data;
//
//     S64 id = sqlite3_column_int64(stmt, 0);
//
//     S64 atlas_id = sqlite3_column_int64(stmt, 1);
//     U32 atlas_idx = (U32)sqlite3_column_int(stmt, 2);
//
//     // String path = string_copy(arena, sqlite3_column_text(stmt, 3));
//     String filename = string_copy(arena, sqlite3_column_text(stmt, 4));
//     ByteSize size = size_to_bytesize(sqlite3_column_int64(stmt, 5));
//     Time ctime = timestamp_to_time(sqlite3_column_int64(stmt, 6));
//     Time mtime = timestamp_to_time(sqlite3_column_int64(stmt, 7));
//
//     // F64 distance = sqlite3_column_double(stmt, 8);
//
//     // switch (view.state.sort_basis)
//     // {
//     // case SortType_Path:
//     // case SortType_Filename:
//     //     break;
//     // case SortType_Size:
//     //     break;
//     // case SortType_DateCreated:
//     // case SortType_DateModified:
//     //     break;
//     // default: break;
//     // }
//
//     ImageMetadata image = {
//         .id = id,
//         .atlas_id = atlas_id,
//         .atlas_idx = atlas_idx,
//         .filename = filename,
//         .size = size,
//         .ctime = ctime,
//         .mtime = mtime,
//     };
//
//     da_push(view.back.arena, view.back.images, image);
//
//     // mscbl_log_info("|%3zu|%.*s|%.*s|%07zu|%10zu|%10zu|%02zu|%2.4f|", id, StringSpr(path), StringSpr(filename), size, ctime, mtime, source, distance);
//     // mscbl_log_info("|%3zu|%02d|%2.4f|", id, source, distance);
// }

// struct range_params
// {
//     S64 start, end;
//     tagged_query *queries;
//     U32 ticket;
// };

// ThreadFunc(view_run_fetch)
// {
//
//     Assert(args[0].kind == TPData_S64, "wrong datatype");
//     Assert(args[1].kind == TPData_S64, "wrong datatype");
//     Assert(args[2].kind == TPData_U32, "wrong datatype");
//     Assert(args[3].kind == TPData_Any, "wrong datatype");
//
//     S64 start = args[0].val_s64;
//     S64 end = args[1].val_s64;
//     U32 ticket = args[2].val_u32;
//     tagged_query *queries = (tagged_query *)args[3].val_any;
//
//     if (view.state.ticket != ticket)
//         return;
//
//     view.back.start = start;
//     view.back.end = end;
//     view.back.ticket = view.state.ticket;
//
//     S64 limit = end - start;
//     S64 offset = start;
//
//     if (model.clip && view.state.search_query.size && view.state.embedding.size == 0)
//         view.state.embedding = model_embed_text(view.state.arena, view.state.search_query);
//
//     for (S64 q = 0; q < da_getsize(queries); q++)
//     {
//         tagged_query query = queries[q];
//
//         // set count
//         {
//             sqlite3_stmt *stmt = db_prepare(CStrCast(query.count_query));
//
//             view_compile_query(stmt, view.state.embedding, query.query_type);
//             db_run_stmt(stmt, 1, view_set_count, &query.query_type);
//         }
//
//         // fetch results
//         {
//             sqlite3_stmt *stmt = db_prepare(CStrCast(query.fetch_query));
//
//             S32 cursor = view_compile_query(stmt, view.state.embedding, query.query_type);
//             sqlite3_bind_int64(stmt, cursor++, limit);
//             sqlite3_bind_int64(stmt, cursor++, offset);
//
//             U64 returned = db_run_stmt(stmt, 1, view_push_data, &query, view.back.arena);
//
//             limit -= returned;
//             offset += returned;
//             if (offset >= view.back.count[q])
//                 offset = 0;
//         }
//     }
//
//     ViewResult temp = view.main;
//     view.main = view.back;
//     view.back = temp;
// }

DBStmtCbk(push_image_map)
{
    arena = ui_search.back.arena;

    switch (ui_state.view_query.sort_basis)
    {
    case SortType_Directory:
    case SortType_Filename:
        da_push(arena, ui_search.back.str_headers, string_copy(arena, sqlite3_column_text(stmt, 0)));
        break;
    case SortType_Size:
        da_push(arena, ui_search.back.size_headers, size_to_bytesize(sqlite3_column_int64(stmt, 0)));
        break;
    case SortType_DateAdded:
    case SortType_DateCreated:
    case SortType_DateModified:
        da_push(arena, ui_search.back.time_headers, timestamp_to_time(sqlite3_column_int64(stmt, 0)));
        break;
    default:
        break;
    }

    da_push(arena, ui_search.back.item_counts, sqlite3_column_int64(stmt, 1));
}

ThreadFunc(view_execute)
{
    Assert(args[0].kind == TPData_String, "wrong datatype");
    Assert(args[1].kind == TPData_B8, "wrong datatype");

    sqlite3_stmt *stmt = db_prepare(CStrCast(args[0].val_str));

    S32 cursor = 1;
    switch (ui_state.view_query.query_type)
    {
    case QueryType_Default:
        switch (ui_state.view_query.sort_basis)
        {
        case SortType_Size: {
            U64 bytes = ui_state.view_query.sub_type;
            sqlite3_bind_int64(stmt, cursor++, bytes);
            sqlite3_bind_int64(stmt, cursor++, bytes);
        }
        break;
        case SortType_DateAdded:
        case SortType_DateCreated:
        case SortType_DateModified: {
            U64 time = ui_state.view_query.sub_type;
            sqlite3_bind_int64(stmt, cursor++, time);
            sqlite3_bind_int64(stmt, cursor++, time);
        }
        break;
        default:
            break;
        }
        break;
        // case QueryType_Embedding:
        //     sqlite3_bind_blob(stmt, cursor++, embedding.vector, embedding.size * sizeof(F32), SQLITE_STATIC);
        //     break;
        // case QueryType_FTS:
        //     sqlite3_bind_text(stmt, cursor++, CStrCast(view.state.search_query), view.state.search_query.size, SQLITE_STATIC);
        // break;
    default:
        break;
    }

    U64 bytes = 0;
    for (UIFilter *f0 = ui_state.view_query.filters; f0 != NULL; f0 = f0->next)
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
        case FilterType_EmbeddingDistanceBetween:
            if (ui_state.view_query.query_type != QueryType_Embedding)
                continue;
            if (f0->val_float.from_enable)
                sqlite3_bind_double(stmt, cursor++, f0->val_float.from);
            if (f0->val_float.to_enable)
                sqlite3_bind_double(stmt, cursor++, f0->val_float.to);
            break;
        default:
            break;
        }
    }

    if (ui_state.view_query.selected_dir)
    {
        sqlite3_bind_int(stmt, cursor++, ui_state.view_query.selected_dir);
    }

    if (args[1].val_b8) // count_only check
    {
        db_run_stmt(stmt, 1, push_image_map);
        S64 header_count = 0;
        switch (ui_state.view_query.sort_basis)
        {
        case SortType_Directory:
        case SortType_Filename:
            header_count = da_getsize(ui_search.back.str_headers);
            break;
        case SortType_Size:
            header_count = da_getsize(ui_search.back.size_headers);
            break;
        case SortType_DateAdded:
        case SortType_DateCreated:
        case SortType_DateModified:
            header_count = da_getsize(ui_search.back.time_headers);
            break;
        default:
            break;
        }
        da_setcap(ui_search.back.arena, ui_search.back.y_offset, header_count + 1);
        da_setcap(ui_search.back.arena, ui_search.back.global_group_offset, header_count + 1);
        ui_search.back.arena_stop = arena_get(ui_search.back.arena);
    }
    else
    {
    }

    UIImageResult temp = ui_search.back;
    ui_search.back = ui_search.main;
    ui_search.main = temp;
}

void view_fetch_map()
{
    Arena *arena = ui_search.back.arena;
    ui_search.back = {0};
    arena_clear(arena);
    ui_search.back.arena = arena;

    StringArr filters = view_serialize_filters(arena, ui_state.view_query.filters);
    String query = {0};

    switch (ui_state.view_query.query_type)
    {
    case QueryType_Default:
        query = view_build_default_query(ui_state.view_query, arena, filters, true);
        break;
        // case QueryType_Embedding:
        //     query = view_build_embedding_query(ui_state.view_query, arena, filters, true);
        // break;
        // case QueryType_FTS:
        //     query = view_build_fts_query(ui_state.view_query, arena, filters, true);
        // break;
    default:
        break;
    }

    if (!query.size)
        return;

    mscbl_log_info("%.*s", StringSpr(query));

    AsyncTask task = {
        .func = view_execute,
        .args = {
            {.kind = TPData_String, .val_str = query},
            {.kind = TPData_B8, .val_b8 = 1},
        }};
    threadpool_enqueue(TaskPriority_Realtime, task);
}

// void view_fetch_range()
// {
//     if (ins_atomic_u32_eval_cond_assign(&view.busy, 1, 0) == 1)
//         return;
//
//     AsyncTask query_task = {.func = 0};
//
//     tagged_query *queries = NULL;
//     S64 start = ui_state.view_clip.start * VIEW_FETCH_WINDOW_SIZE;
//     S64 end = MAX(ui_state.view_clip.end, 1) * VIEW_FETCH_WINDOW_SIZE;
//
//     arena_clear(view.back.arena);
//     view.back.images = 0;
//     view.back.start = 0;
//     view.back.end = 0;
//     if (view.main.start <= start && view.main.end >= end && view.state.ticket == view.main.ticket)
//         goto Cleanup;
//
//     for (U32 q = 0; q < QueryType_COUNT; q++)
//         view.back.count[q] = 0;
//
//     if (view.state.search_query.size)
//     {
//         // tagged_query qry = {
//         //     .count_query = view_build_query(QueryType_Embedding, true),
//         //     .fetch_query = view_build_query(QueryType_Embedding),
//         //     .query_type = QueryType_Embedding};
//         // da_push(view.back.arena, queries, qry);
//
//         tagged_query qry = {
//             .count_query = view_build_query(QueryType_FTS, true),
//             .fetch_query = view_build_query(QueryType_FTS),
//             .query_type = QueryType_FTS};
//         da_push(view.back.arena, queries, qry);
//     }
//     else
//     {
//         tagged_query qry = {
//             .count_query = view_build_query(QueryType_Default, true),
//             .fetch_query = view_build_query(QueryType_Default),
//             .query_type = QueryType_Default};
//         da_push(view.back.arena, queries, qry);
//     }
//     query_task = {
//         .func = view_run_fetch,
//         .args = {
//             {.kind = TPData_S64, .val_s64 = start},
//             {.kind = TPData_S64, .val_s64 = end},
//             {.kind = TPData_U32, .val_u32 = view.state.ticket},
//             {.kind = TPData_Any, .val_any = queries},
//         }};
//     threadpool_enqueue(TaskPriority_Realtime, query_task);
//
// Cleanup:
//     ins_atomic_u32_eval_assign(&view.busy, 0);
// }

// void view_refresh()
// {
//     view.state.ticket++;
//     view_fetch_range();
// }

// ViewResult view_get_result()
// {
//     return view.main;
// }
