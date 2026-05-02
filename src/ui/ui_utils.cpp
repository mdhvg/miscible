#include "base/arena.h"
#include "ui/ui_core.h"
#include "ui/ui_utils.h"
#include "ui/pages/menu/menu.h"

Arena *ui_arena = NULL;

const char *byte_size[] = {"B", "KB", "MB", "GB", "PB"};

SizeUnits formatted_size(U64 size)
{
    U64 size_int   = size;
    F32 size_float = 0;
    U8 unit_ptr    = 0;
    while (size_int >= 1024)
    {
        size_float = (size_int % 1024) / 1000.0f;
        size_int /= 1024;
        unit_ptr += 1;
    }
    F32 final_size = (float)size_int + size_float;
    return {final_size, byte_size[unit_ptr]};
}

// DBStmtCbk(push_id)
// {
//     U64 id        = sqlite3_column_int64(stmt, 0);
//     Image_Node *n = tree_find(&index.metadata, &id, Image_cmp, Image);
//     Image *iptr   = &n->v;
//     da_push(index_arena, index.order, iptr);
// }
//
// void index_fetch_order(sort_params params)
// {
//     PERF_BEGIN(Indexing);
//     index.order = NULL;
//     Assert(params.arena, "no arena for index fetch");
//     const char *order_by = order_str[params.order_by];
//     const char *arrange  = params.ascending ? "ASC" : "DESC";
//
//     StringBuilder query = string_empty(params.arena);
//     string_format(&query, "SELECT id FROM Images ORDER BY %s %s", order_by, arrange);
//     // string_push(params.arena, query, /* Order By */);
//     sqlite3_stmt *stmt = db_prepare(CStrCast(query));
//     db_run_stmt(stmt, 1, push_id);
//     PERF_END(Indexing);
// }

// void refresh_results()
// {
//     ui_state.images = NULL;
//     arena_clear(ui_arena);
//
//     index_fetch_order(current_sort);
//     U64 count = da_getsize(index.order);
//     da_setcap(ui_arena, ui_state.images, count);
//
//     for (U64 i = 0; i < count; i++)
//     {
//         Image img_ptr = *index.order[i];
//         da_push(ui_arena, ui_state.images, img_ptr);
//     }
// }

#if DBG
global_v LibHandle pages = 0;
global_v UIfn restyle    = NULL;

void ui_reload()
{
    if (pages)
        os_closelib(pages);
    pages   = os_loadlib("pages" LibExt);
    restyle = (UIfn)os_libfunc(pages, "restyle");

    for (S32 i = 0; i < UIPage_COUNT; i++)
    {
        page_data[i].fn = (UIfn)os_libfunc(pages, page_data[i].fn_name);
    }
    arena_clear(ui_state.arena);
}
#endif
