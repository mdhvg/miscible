#include "db/view.h"
#include "base/log.h"
#include "db/db_helpers.h"
#include "base/array.h"
#include "base/ringbuf.h"
#include "base/string.h"
#include "base/threadpool.h"

ViewManager view = {0};

void view_init()
{
    arena_alloc(MB(1), view.main.arena);
    arena_alloc(MB(1), view.back.arena);

    arena_alloc(MB(1), view.state.arena);
    view.state.query.search_query = string_empty(view.state.arena, 4096);

    InitializeCriticalSection(&view.queue_mutex);
}

ThreadFunc(view_process)
{
    EnterCriticalSection(&view.queue_mutex);
    ViewRequest req = rb_pop(view.request_queue);
    LeaveCriticalSection(&view.queue_mutex);

    if (req.ticket < view.ticket_counter)
        return;
}

void view_update_search(String query)
{
    StringBuilder *sq = &view.state.query.search_query;
    if (string_cmp(StringCast(*sq), query))
        string_assign(sq, query);
}

void view_push_filters(ViewFilter *filters)
{
    for (ViewFilter *f = filters; f != NULL; f = f->next)
    {
        ViewFilter f1 = {.type = f->type, .exclude = f->exclude};
        switch (f->type)
        {
        case FilterType_Path:
        case FilterType_Filename:
            f1.val_str = string_cpy(view.state.arena, f->val_str);
            break;

        case FilterType_SizeGreater:
        case FilterType_DateCreatedAfter:
        case FilterType_DateModifiedAfter:
            f1.val_int = f->val_int;
            break;

        default:
            break;
        }
        da_push(view.state.arena, view.state.query.filters, f1);
    }
}

SortType view_get_order()
{
    return view.state.query.sort_basis;
}

void view_set_order(SortType sort)
{
    view.state.query.sort_basis = sort;
}

String *view_serialize_filters(Arena *arena, ViewFilter *filters)
{
    String *f1 = NULL;

    for (S64 i = 0; i < da_getsize(filters); i++)
    {
        ViewFilter f0 = filters[i];
        switch (f0.type)
        {
        case FilterType_Path:
        case FilterType_Filename:
            da_push(arena, f1, sv(" AND id"));
            if (f0.exclude)
                da_push(arena, f1, sv(" NOT"));
            da_push(arena, f1, sv(" in (SELECT rowid FROM Image_FTS WHERE Image_FTS MATCH ?)"));
            break;
        case FilterType_SizeGreater:
            if (!f0.exclude)
                da_push(arena, f1, sv(" AND size >= ?"));
            else
                da_push(arena, f1, sv(" AND size < ?"));
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

String view_build_query(ViewRequest request)
{
    Arena *arena    = request.arena;
    String *queries = NULL;

    String *filters = view_serialize_filters(arena, request.query.filters);

    // TODO: Make code for getting embedding and creating `distance` variable
    da_push(arena, queries, sv("SELECT id, 0 as source FROM Images WHERE embedding IS NOT NULL"));
    for (S64 i = 0; i < da_getsize(filters); i++)
        da_push(arena, queries, filters[i]);

    da_push(arena, queries, sv(" UNION ALL "));

    da_push(arena, queries, sv("SELECT id, 1 as source FROM Images WHERE id IN (SELECT rowid FROM Images_FTS WHERE Images_FTS MATCH ?)"));
    for (U32 i = 0; i < da_getsize(filters); i++)
        da_push(arena, queries, filters[i]);

    da_push(arena, queries, sv(" ORDER BY source ASC,"));
    switch (request.query.sort_basis)
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

    da_push(arena, queries, request.query.descending ? sv(" DESC;") : sv(" ASC;"));

    StringBuilder query = string_empty(arena, KB(4));
    for (S64 i = 0; i < da_getsize(queries); i++)
        string_push(&query, queries[i]);

    mscbl_log_dbg("--- FINAL QUERY ---\n%.*s\n--- END ---", StringSpr(query));

    return {0};
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

void view_reload()
{
    view_build_query(view.state);
    // ViewRequest req = {
    //     // .embedding_dim = // model.get ... embedding_dim
    //     // .search_embedding = // Done later
    //     .query  = view.state.query,
    //     .ticket = ++view.ticket_counter,
    // };
    //
    // EnterCriticalSection(&view.queue_mutex);
    // rb_push(view.request_queue, req);
    // LeaveCriticalSection(&view.queue_mutex);
    //
    // threadpool_enqueue({view_process});
}

Arena *view_arena = NULL;
S64 *view_order   = NULL;

DBStmtCbk(push_id)
{
    da_push(view_arena, view_order, sqlite3_column_int64(stmt, 0));
}

void view_fetch()
{
    if (!view_arena)
        arena_alloc(MB(1), view_arena);
    arena_clear(view_arena);

    view_order         = NULL;
    sqlite3_stmt *stmt = db_prepare("SELECT id FROM Images ORDER BY path ASC;");
    db_run_stmt(stmt, 1, push_id);
}
//
// void view_set_order(ViewOrder order)
// {
//     view_params.order = order;
//     view_fetch();
// }
