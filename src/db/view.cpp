#include "db/view.h"
#include "base/arena.h"
#include "base/base_core.h"
#include "base/log.h"
#include "config.h"
#include "db/db_helpers.h"
#include "base/array.h"
#include "base/string.h"
#include "base/threadpool.h"
#include "inference/model.h"
#include "sqlite3.h"
#include "ui/ui_core.h"

ViewManager view = {0};

void view_init()
{
    arena_alloc(MB(1), view.main.arena);
    arena_alloc(MB(1), view.back.arena);
    arena_alloc(MB(1), view.state.arena);
    view.busy = 0;

    view_clear_state();
}

void view_clear_state()
{
    if (ins_atomic_u32_eval_cond_assign(&view.busy, 1, 0) == 1)
        return;

    view.state.search_query = {0};
    view.state.sort_basis = mscbl_config.view_settings.sort_basis;
    view.state.descending = mscbl_config.view_settings.descending;
    view.state.filters = 0;

    arena_clear(view.state.arena);

    ins_atomic_u32_eval_assign(&view.busy, 0);
}

void view_set_state(UIViewQuery ui_query)
{
    if (ins_atomic_u32_eval_cond_assign(&view.busy, 1, 0) == 1)
        return;

    view.state.search_query = string_copy(view.state.arena, StringCast(ui_query.search_query));
    view.state.sort_basis = (SortType)ui_query.sort_basis;
    view.state.descending = ui_query.descending;

    for (UIFilter *f0 = ui_query.filters; f0 != NULL; f0 = f0->next)
    {
        if (!f0->active)
            continue;

        ViewFilter f1 = {.type = f0->type, .exclude = f0->exclude};
        switch (f0->type)
        {
        case FilterType_SizeGreater:
            f1.val_bytes = f0->val_bytes;
            break;

        case FilterType_Path:
            f1.val_str = string_copy(view.state.arena, StringCast(f0->val_str));
            break;

        case FilterType_DateCreatedAfter:
        case FilterType_DateModifiedAfter:
            f1.val_date = f0->val_date;
            break;

        default:
            break;
        }
        da_push(view.state.arena, view.state.filters, f1);
    }

    ins_atomic_u32_eval_assign(&view.busy, 0);
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

String view_build_default_query(ViewQuery request, Arena *arena, String *filters)
{
    String *queries = NULL;

    da_push(arena, queries, sv("SELECT id, path, filename, size, ctime, mtime, 0 as distance FROM Images WHERE 1=1"));

    for (U32 i = 0; i < da_getsize(filters); i++)
        da_push(arena, queries, filters[i]);

    da_push(arena, queries, sv(" ORDER BY"));

    switch (request.sort_basis)
    {
    case SortType_Path:
        da_push(arena, queries, sv(" path"));
        break;
    case SortType_Filename:
        da_push(arena, queries, sv(" filename"));
        break;
    case SortType_Size:
        da_push(arena, queries, sv(" size"));
        break;
    case SortType_DateCreated:
        da_push(arena, queries, sv(" ctime"));
        break;
    case SortType_DateModified:
        da_push(arena, queries, sv(" mtime"));
        break;
    default:
        Assert(0, "invalid sort order");
        break;
    }

    da_push(arena, queries, request.descending ? sv(" DESC;") : sv(" ASC;"));

    StringBuilder query = string_empty(arena, KB(4));
    for (S64 i = 0; i < da_getsize(queries); i++)
        string_push(&query, queries[i]);

    mscbl_log_dbg("Query: %.*s", StringSpr(query));

    return StringCast(query);
}

String view_build_fts_query(ViewQuery request, Arena *arena, String *filters)
{
    String *queries = NULL;

    da_push(arena, queries, sv("SELECT id, path, filename, size, ctime, mtime, 0 as distance FROM Images WHERE id IN (SELECT rowid FROM Image_FTS WHERE Image_FTS MATCH ?)"));

    for (U32 i = 0; i < da_getsize(filters); i++)
        da_push(arena, queries, filters[i]);

    da_push(arena, queries, sv(" ORDER BY"));

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
    default:
        Assert(0, "invalid sort order");
        break;
    }

    da_push(arena, queries, request.descending ? sv(" DESC;") : sv(" ASC;"));

    StringBuilder query = string_empty(arena, KB(4));
    for (S64 i = 0; i < da_getsize(queries); i++)
        string_push(&query, queries[i]);

    mscbl_log_dbg("Query: %.*s", StringSpr(query));

    return StringCast(query);
}

String view_build_embedding_query(ViewQuery request, Arena *arena, String *filters)
{
    String *queries = NULL;

    da_push(arena, queries, sv("SELECT id, path, filename, size, ctime, mtime, distance_cosine_f32(embedding, ?) AS distance FROM Images WHERE embedding IS NOT NULL"));

    for (S64 i = 0; i < da_getsize(filters); i++)
        da_push(arena, queries, filters[i]);

    da_push(arena, queries, sv(" ORDER BY distance ASC;"));

    StringBuilder query = string_empty(arena, KB(4));
    for (S64 i = 0; i < da_getsize(queries); i++)
        string_push(&query, queries[i]);

    mscbl_log_dbg("Query: %.*s", StringSpr(query));

    return StringCast(query);
}

struct tagged_query
{
    String query;
    QueryType query_type;
};

tagged_query view_build_query(ViewQuery request, QueryType query_type)
{
    Arena *arena = request.arena;
    String *filters = view_serialize_filters(arena, request.filters);

    String query = {0};
    switch (query_type)
    {
    case QueryType_None:
        query = view_build_default_query(request, arena, filters);
        break;
    case QueryType_Embedding:
        query = view_build_embedding_query(request, arena, filters);
        break;
    case QueryType_FTS:
        query = view_build_fts_query(request, arena, filters);
        break;
    default:
        Assert(0, "wrong enum type %d", query_type);
    }
    return {.query = query, .query_type = query_type};
}

DBStmtCbk(view_push_result)
{
    QueryType query_type = *(QueryType *)data;

    S64 id = sqlite3_column_int64(stmt, 0);
    // String path = sv(sqlite3_column_text(stmt, 1));
    // String filename = sv(sqlite3_column_text(stmt, 2));
    // S64 size = sqlite3_column_int64(stmt, 3);
    // S64 ctime = sqlite3_column_int64(stmt, 4);
    // S64 mtime = sqlite3_column_int64(stmt, 5);
    F64 distance = sqlite3_column_double(stmt, 6);

    S64 push_idx = da_getsize(view.back.image_ids);
    da_push(view.back.arena, view.back.image_ids, id);

    if (da_getsize(view.back.groups) > (U32)query_type)
    {
        ViewResultGroup *group = &view.back.groups[query_type];
        if (group->count == 0)
            *group = {
                .start_index = push_idx,
                .count = 1,
                .query_type = query_type};

        group->count++;
    }
    else
    {
        ViewResultGroup dummy = {0};
        while (da_getsize(view.back.groups) < (U32)query_type)
            da_push(view.back.arena, view.back.groups, dummy);

        ViewResultGroup group = {
            .start_index = push_idx,
            .count = 1,
            .query_type = query_type};
        da_push(view.back.arena, view.back.groups, group);
    }

    // mscbl_log_dbg("|%3zu|%.*s|%.*s|%07zu|%10zu|%10zu|%02zu|%2.4f|", id, StringSpr(path), StringSpr(filename), size, ctime, mtime, source, distance);
    // mscbl_log_dbg("|%3zu|%02d|%2.4f|", id, source, distance);
}

ThreadFunc(view_run_query)
{
    Assert(data.kind == TPData_ANY, "wrong datatype");
    tagged_query *queries = (tagged_query *)data.val_any;
    ViewFilter *filters = view.state.filters;

    view.back.groups = NULL;
    view.back.image_ids = NULL;
    arena_clear(view.back.arena);

    for (S64 q = 0; q < da_getsize(queries); q++)
    {
        tagged_query query = queries[q];

        sqlite3_stmt *stmt = db_prepare(CStrCast(query.query));
        S32 cursor = 1;
        Embedding embedding;

        switch (query.query_type)
        {
        case QueryType_None:
            break;
        case QueryType_Embedding:
            if (!model.clip)
                continue;
            embedding = model_embed_text(arena, StringCast(view.state.search_query));
            sqlite3_bind_blob(stmt, cursor++, embedding.vector, embedding.size * sizeof(F32), SQLITE_STATIC);
            break;
        case QueryType_FTS:
            sqlite3_bind_text(stmt, cursor++, CStrCast(view.state.search_query), view.state.search_query.size, SQLITE_STATIC);
            break;
        default:
            Assert(0, "wrong enum type %d", query.query_type);
        }

        U64 bytes = 0;
        for (S64 i = 0; i < da_getsize(filters); i++)
        {
            ViewFilter f0 = filters[i];
            switch (f0.type)
            {
            case FilterType_SizeGreater:
                bytes = (f0.val_bytes.value * (1 << (10 * (int)f0.val_bytes.unit)));
                sqlite3_bind_int64(stmt, cursor, bytes);
                break;
            case FilterType_Path:
                sqlite3_bind_text(stmt, cursor, CStrCast(f0.val_str), f0.val_str.size, SQLITE_STATIC);
                break;
            case FilterType_DateCreatedAfter:
            case FilterType_DateModifiedAfter:
                sqlite3_bind_int64(stmt, cursor, date_to_timestamp(f0.val_date));
                break;
            case FilterType_EmbeddingDistanceGreater:
                // TODO: This case
                break;
            default:
                break;
            }
            cursor++;
        }

        db_run_stmt(stmt, 1, view_push_result, &query.query_type);
    }

    ViewResult temp = view.main;
    view.main = view.back;
    view.back = temp;
}

void view_reload()
{
    if (ins_atomic_u32_eval_cond_assign(&view.busy, 1, 0) == 1)
        return;

    tagged_query *queries = NULL;

    if (view.state.search_query.size)
    {
        da_push(view.state.arena, queries, view_build_query(view.state, QueryType_FTS));
        da_push(view.state.arena, queries, view_build_query(view.state, QueryType_Embedding));
    }
    else
    {
        da_push(view.state.arena, queries, view_build_query(view.state, QueryType_None));
    }

    AsyncTask query_task = {
        .func = view_run_query,
        .data = {
            .kind = TPData_ANY,
            .val_any = queries}};
    threadpool_enqueue(TaskPriority_Realtime, query_task);

    ins_atomic_u32_eval_assign(&view.busy, 0);
}

ViewResult view_get_result()
{
    return view.main;
}
