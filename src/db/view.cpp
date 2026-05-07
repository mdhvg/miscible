#include "db/view.h"
#include "base/arena.h"
#include "base/log.h"
#include "config.h"
#include "db/db_helpers.h"
#include "base/array.h"
#include "base/string.h"
#include "base/threadpool.h"
#include "inference/model.h"
#include "ui/ui_core.h"

ViewManager view = {0};

void view_init()
{
    arena_alloc(MB(1), view.main.arena);
    arena_alloc(MB(1), view.back.arena);

    arena_alloc(MB(1), view.state.arena);

    view_clear_state();
}

void view_clear_state()
{
    view.state.search_query = {0};
    view.state.sort_basis = mscbl_config.view_settings.sort_basis;
    view.state.descending = mscbl_config.view_settings.descending;
    view.state.filters = 0;

    arena_clear(view.state.arena);
}

void view_set_state(UIViewQuery ui_query)
{
    view.state.search_query = string_cpy(view.state.arena, StringCast(ui_query.search_query));
    view.state.sort_basis = (SortType)ui_query.sort_basis;
    view.state.descending = ui_query.descending;

    for (S64 i = 1; i < da_getsize(ui_query.filters); i++)
    {
        UIFilter f0 = ui_query.filters[i];
        ViewFilter f1 = {.type = f0.type, .exclude = f0.exclude};
        switch (f0.type)
        {
        case FilterType_SizeGreater:
            f1.val_bytes = f0.val_bytes;
            break;

        case FilterType_Path:
        case FilterType_Filename:
            f1.val_str = string_cpy(view.state.arena, StringCast(f0.val_str));
            break;

        case FilterType_DateCreatedAfter:
        case FilterType_DateModifiedAfter:
            f1.val_date = f0.val_date;
            break;

        default:
            break;
        }
        da_push(view.state.arena, view.state.filters, f1);
    }
}

String *view_serialize_filters(Arena *arena, ViewFilter *filters)
{
    String *f1 = NULL;

    for (S64 i = 0; i < da_getsize(filters); i++)
    {
        ViewFilter f0 = filters[i];
        switch (f0.type)
        {
        case FilterType_SizeGreater:
            if (!f0.exclude)
                da_push(arena, f1, sv(" AND size >= ?"));
            else
                da_push(arena, f1, sv(" AND size < ?"));
            break;
        case FilterType_Path:
        case FilterType_Filename:
            da_push(arena, f1, sv(" AND id"));
            if (f0.exclude)
                da_push(arena, f1, sv(" NOT"));
            da_push(arena, f1, sv(" in (SELECT rowid FROM Image_FTS WHERE Image_FTS MATCH ?)"));
            break;
        case FilterType_DateCreatedAfter:
            if (!f0.exclude)
                da_push(arena, f1, sv(" AND ctime >= ?"));
            else
                da_push(arena, f1, sv(" AND ctime < ?"));
            break;
        case FilterType_DateModifiedAfter:
            if (!f0.exclude)
                da_push(arena, f1, sv(" AND mtime >= ?"));
            else
                da_push(arena, f1, sv(" AND mtime < ?"));
            break;
        case FilterType_EmbeddingDistanceGreater:
            if (!f0.exclude)
                da_push(arena, f1, sv(" AND distance >= ?"));
            else
                da_push(arena, f1, sv(" AND distance < ?"));
            break;
        default:
            break;
        }
    }

    return f1;
}

String view_build_query(ViewQuery request)
{
    Arena *arena = request.arena;
    String *queries = NULL;

    String *filters = view_serialize_filters(arena, request.filters);

    if (request.search_query.size)
    {
        da_push(arena, queries, sv("SELECT id, path, filename, size, ctime, mtime, 1 as source, distance_cosine_f32(embedding, ?) AS distance FROM Images WHERE embedding IS NOT NULL"));

        for (S64 i = 0; i < da_getsize(filters); i++)
            da_push(arena, queries, filters[i]);

        da_push(arena, queries, sv(" UNION ALL"));
    }

    if (request.search_query.size)
    {
        da_push(arena, queries, sv(" SELECT id, path, filename, size, ctime, mtime, 2 as source, 0 as distance FROM Images WHERE id IN (SELECT rowid FROM Image_FTS WHERE Image_FTS MATCH ?)"));
    }
    else
    {
        da_push(arena, queries, sv(" SELECT id, path, filename, size, ctime, mtime, 0 as source, 0 as distance FROM Images WHERE 1=1"));
    }

    for (U32 i = 0; i < da_getsize(filters); i++)
        da_push(arena, queries, filters[i]);

    da_push(arena, queries, sv(" ORDER BY source ASC,"));

    switch (request.sort_basis)
    {
    case SortType_Path:
        da_push(arena, queries, sv(" path"));
        break;
    case SortType_Size:
        da_push(arena, queries, sv(" size"));
        break;
    case SortType_Filename:
        da_push(arena, queries, sv(" filename"));
        break;
    case SortType_DateCreated:
        da_push(arena, queries, sv(" ctime"));
        break;
    case SortType_DateModified:
        da_push(arena, queries, sv(" mtime"));
        break;
    case SortType_EmbeddingDistance:
        da_push(arena, queries, sv(" distance"));
        break;
    default:
        Assert(0, "invalid sort order");
        break;
    }

    da_push(arena, queries, request.descending ? sv(" DESC;") : sv(" ASC;"));

    StringBuilder query = string_empty(arena, KB(4));
    for (S64 i = 0; i < da_getsize(queries); i++)
        string_push(&query, queries[i]);

    return StringCast(query);
}

// void view_reload()
// {
// Make a query request (with embedding if required)
// generate sqlite3 query
// perform search
// update results
// }

// void add_filter(ViewFilter filter)
// {
//     da_push(view.arena, view.state.filters, filter);
// }

void view_fill_filters(sqlite3_stmt *stmt, S32 *cursor, ViewFilter *filters)
{
    U64 bytes = 0, timestamp = 0;
    for (S64 i = 0; i < da_getsize(filters); i++)
    {
        ViewFilter f0 = filters[i];
        switch (f0.type)
        {
        case FilterType_SizeGreater:
            bytes = (f0.val_bytes.value * (1 << (10 * (int)f0.val_bytes.unit)));
            sqlite3_bind_int64(stmt, *cursor, bytes);
            break;
        case FilterType_Path:
        case FilterType_Filename:
            sqlite3_bind_text(stmt, *cursor, CStrCast(f0.val_str), f0.val_str.size, SQLITE_STATIC);
            break;
        case FilterType_DateCreatedAfter:
        case FilterType_DateModifiedAfter:
            // TODO: Make UNIX timestamp (also, convert os timestamps to UNIX timestamp in Windows functions)
            timestamp = 0;
            sqlite3_bind_int64(stmt, *cursor, timestamp);
            break;
        case FilterType_EmbeddingDistanceGreater:
            // TODO: This case
            break;
        default:
            break;
        }
        *cursor += 1;
    }
}

DBStmtCbk(print_rows)
{
    S64 id = sqlite3_column_int64(stmt, 0);
    // String path = sv(sqlite3_column_text(stmt, 1));
    // String filename = sv(sqlite3_column_text(stmt, 2));
    // S64 size = sqlite3_column_int64(stmt, 3);
    // S64 ctime = sqlite3_column_int64(stmt, 4);
    // S64 mtime = sqlite3_column_int64(stmt, 5);
    S32 source = sqlite3_column_int(stmt, 6);
    F64 distance = sqlite3_column_double(stmt, 7);

    S64 push_idx = da_getsize(view.back.image_ids);
    da_push(view.back.arena, view.back.image_ids, id);

    if (da_getsize(view.back.groups) > source)
    {
        view.back.groups[source].count++;
    }
    else
    {
        ViewResultGroup dummy = {0};
        while (da_getsize(view.back.groups) < source)
            da_push(view.back.arena, view.back.groups, dummy);
        ViewResultGroup group = {
            .start_index = push_idx,
            .count = 1,
            .source = (Source)source};
        da_push(view.back.arena, view.back.groups, group);
    }

    // mscbl_log_dbg("|%3zu|%.*s|%.*s|%07zu|%10zu|%10zu|%02zu|%2.4f|", id, StringSpr(path), StringSpr(filename), size, ctime, mtime, source, distance);
    mscbl_log_dbg("|%05zu|%02d|%2.4f|", id, source, distance);
}

ThreadFunc(view_run_query)
{
    Assert(data.kind == TPData_String, "wrong datatype");
    String query = data.str;
    sqlite3_stmt *stmt = db_prepare(CStrCast(query));
    S32 cursor = 1;

    if (view.state.search_query.size)
    {
        Embedding embedding = embed_text(StringCast(view.state.search_query));
        sqlite3_bind_blob(stmt, cursor++, embedding.vector, embedding.size * sizeof(F32), SQLITE_STATIC);
        view_fill_filters(stmt, &cursor, view.state.filters);
    }

    if (view.state.search_query.size)
        sqlite3_bind_text(stmt, cursor++, CStrCast(view.state.search_query), view.state.search_query.size, SQLITE_STATIC);

    view_fill_filters(stmt, &cursor, view.state.filters);

    view.back.groups = NULL;
    view.back.image_ids = NULL;
    arena_clear(view.back.arena);

    db_run_stmt(stmt, 1, print_rows);

    ViewResult temp = view.main;
    view.main = view.back;
    view.back = temp;
}

void view_reload()
{
    String query = view_build_query(view.state);
    AsyncTask query_task = {
        .func = view_run_query,
        .data = {
            .kind = TPData_String,
            .str = query}};
    threadpool_enqueue(query_task);
}

ViewResult view_get_result()
{
    return view.main;
}

// Arena *view_arena = NULL;
// S64 *view_order = NULL;
//
// DBStmtCbk(push_id)
// {
//     da_push(view_arena, view_order, sqlite3_column_int64(stmt, 0));
// }
//
// void view_fetch()
// {
//     if (!view_arena)
//         arena_alloc(MB(1), view_arena);
//     arena_clear(view_arena);
//
//     view_order = NULL;
//     sqlite3_stmt *stmt = db_prepare("SELECT id FROM Images ORDER BY path ASC;");
//     db_run_stmt(stmt, 1, push_id);
// }
